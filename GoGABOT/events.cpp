#include <thread>
#include <limits.h>
#include <iostream>
#include <chrono>
#include <math.h>
#include <mutex>
#include <condition_variable>
#include "events.h"
#include "gt.hpp"
#include "proton/hash.hpp"
#include "proton/rtparam.hpp"
#include "proton/variant.hpp"
#include "server.h"
#include "utils.h"
#include "world.h"

using namespace std;

bool events::send::VariantList(gameupdatepacket_t *packet)
{
    variantlist_t varlist{};
    auto extended = utils::get_extended(packet);
    extended += 4; //since it casts to data size not data but too lazy to fix this
    varlist.serialize_from_mem(extended);
    // PRINTD("varlist: %s\n", varlist.print().c_str());
    auto func = varlist[0].get_string();
    // probably subject to change, so not including in switch statement.
    if (func.find("OnSuperMainStartAcceptLogon") != -1) gt::in_game = true;
    
    switch (hs::hash32(func.c_str()))
    {
        case fnv32("onShowCaptcha"):
        {

            // TODO: - need to improve
            // gt::solve_captcha(varlist[1].get_string());
            return true;
        }
            break;
        case fnv32("OnRequestWorldSelectMenu"):
        {
            auto &world = g_server->m_world;
            world.players.clear();
            world.local = {};
            world.connected = false;
            world.name = "EXIT";
            gt::is_admin_entered = false;
            MenuBar::refreshStatusWindow(TYPE_WORLD);
            g_server->DroppedItem.clear();
            g_server->adminNetID.clear();
            g_server->setTargetWorld(gt::target_world_name);
        }
            break;
        case fnv32("OnSendToServer"):
            g_server->redirect_server(varlist);
            return true;
        case fnv32("OnConsoleMessage"):
        {
            string text = varlist[1].get_string();
            string stripText = utils::stripMessage(text);
            string playerName = utils::getValueFromPattern(text, "`6<`w(.*)``>``");
            
            if (stripText.find("Oops, " + utils::toUpper(gt::target_world_name) + " already has") != -1 || stripText.find("Destination has too many players") != -1)
            {
                // We need set delay to this because packet lost from client
                this_thread::sleep_for(chrono::seconds(2));
                g_server->setTargetWorld(gt::target_world_name);
            } else if(stripText.find(">>Spam detected!") != -1) {
                gt::safety_spam = 4;
            } else if (stripText.find("world bans") != -1) {
                gt::is_exit = utils::toUpper(utils::getValueFromPattern(stripText, "world bans (.*) from")) == utils::toUpper(gt::bot_name);
            }
            
            /* ********************* Optional ************************* */
            
            // Auto ban people with bad word list
            if (gt::is_auto_ban && !playerName.empty() && (utils::toUpper(stripText).find("SELL") != -1 ||
                                                           utils::toUpper(stripText).find("GO ") != -1 ||
                                                           utils::toUpper(stripText).find("SCAM") != -1 ||
                                                           utils::toUpper(stripText).find("GOTO ") != -1 ||
                                                           utils::toUpper(stripText).find("SKEM") != -1))
            {
                g_server->send("action|input\n|text|/ban " + playerName);
            }
            
            // Auto accept access
            if (stripText.find("Wrench yourself to accept.") != -1) {
                string netID = to_string(g_server->m_world.local.netid);
                g_server->send("action|wrench\n|netid|" + netID);
                g_server->send("action|dialog_return\ndialog_name|popup\nnetID|"+ netID +"|\nbuttonClicked|acceptlock");
                g_server->send("action|dialog_return\ndialog_name|acceptaccess");
            }
            
            // Collecting droping items
            if (text.find("Collected `w") != -1 && stripText.find("[W]_") == -1) {
                bool isSeed = text.find("Seed") != -1;
                World::ItemDefinitionStruct itemInfo = g_server->m_world.GetItemDef(isSeed ? gt::block_id+1 : gt::block_id);
                if (stripText.find(itemInfo.itemName) != -1) {
                    int totalCollectedItem = atoi(utils::getValueFromPattern(text, "Collected `w(.*) " + itemInfo.itemName).c_str());
                    if (isSeed) {
                        g_server->playerInventory.updateTotalDroppedItem(totalCollectedItem);
                    } else {
                        g_server->playerInventory.updateInventoryTotalBlock(totalCollectedItem);
                    }
                    MenuBar::refreshStatusWindow(TYPE_BOTTOM);
                }
            }
            
            // Additional command
            if (stripText.find("[W]_") == -1) return true;
            if (utils::toUpper(gt::owner_name) == utils::toUpper(utils::getValueFromPattern(stripText, "\\[W\\]_ <(.*) \\!"))) {
                if (stripText.find("!follow") != -1) {
                    gt::is_following_owner = !gt::is_following_owner;
                } else if (stripText.find("!warp") != -1) {
                    string worldName = utils::stripMessage(utils::split(text, "!warp", 1));
                    vector<string> targetWorld = utils::split(worldName, " ");
                    gt::target_world_name = targetWorld.size() <= 1 ? targetWorld[0] : targetWorld[0] + "|" + targetWorld[1];
                    g_server->setTargetWorld(gt::target_world_name);
                } else if (stripText.find("!setMsg") != -1) {
                    gt::spam_text = utils::split(text, "!setMsg", 1);
                }
            }
            
            /* ********************* Optional ************************* */
            
            return true;
        }
            break;
        case fnv32("OnDialogRequest"):
        {
            auto content = varlist[1].get_string();
            if (content.find("set_default_color|`o") != -1)
            {
                if (content.find("end_dialog|captcha_submit||Submit|") != -1)
                {
                    gt::solve_captcha(content);
                    return true;
                } else if (content.find("add_label_with_icon|big|`wThe Growtopia Gazette") != -1) {
                    g_server->send("action|dialog_return\ndialog_name|gazette");
                    return true;
                } else if (content.find("add_label_with_icon|big|`wDonation Box") != -1) {
                    if (content.find("The box is currently empty.") != -1) {
                        if (g_server->playerInventory.getTotalDroppedItem() > 0) {
                            this_thread::sleep_for(chrono::seconds(2));
                            g_server->send("action|drop\n|itemID|" + to_string(gt::block_id+1));
                        }
                        g_server->donationBoxPosition++;
                        if (g_server->donationBoxPosition == 5) g_server->donationBoxPosition = 0;
                        return true;
                    }
                    int tilePosX = ((int)(g_server->lastPos.m_x / 32) - 2) + g_server->donationBoxPosition;
                    int tilePosY = (int)ceil(g_server->lastPos.m_y / 32) - 2;
                    g_server->send("action|dialog_return\ndialog_name|donation_box_edit\ntilex|"+to_string(tilePosX)+"|\ntiley|"+to_string(tilePosY)+
                                   "|\nbuttonClicked|clear\n\ncheckbox|0");
                    return true;
                }
            }
            if (content.find("embed_data|itemID|") != -1 && content.find("Drop") != -1)
            {
                string itemid = content.substr(content.find("embed_data|itemID|") + 18, content.length() - content.find("embed_data|itemID|") - 1);
                string count = content.substr(content.find("count||") + 7, content.length() - content.find("count||") - 1);
                g_server->send("action|dialog_return\ndialog_name|drop_item\nitemID|" + itemid + "|\ncount|" + count);
                return true;
            } else if (content.find("embed_data|itemID|") != -1 && content.find("Trash") != -1){
                string itemid = content.substr(content.find("embed_data|itemID|") + 18, content.length() - content.find("embed_data|itemID|") - 1);
                string count = content.substr(content.find("you have ") + 9, content.length() - content.find("you have ") - 1);
                string delimiter = ")";
                string token = count.substr(0, count.find(delimiter));
                g_server->send("action|dialog_return\ndialog_name|trash_item\nitemID|" + itemid + "|\ncount|" + token);
                return true;
            }
        }
            break;
        case fnv32("OnRemove"):
        {
            auto text = varlist.get(1).get_string();
            if (text.find("netID|") == 0)
            {
                auto netid = atoi(text.substr(6).c_str());
                
                if (netid == g_server->m_world.local.netid)
                    g_server->m_world.local = {};
                
                auto &players = g_server->m_world.players;
                for (size_t i = 0; i < players.size(); i++)
                {
                    auto &player = players[i];
                    if (player.netid == netid)
                    {
                        if (gt::public_net_id == netid && gt::is_following_public) {
                            gt::public_net_id = -1;
                            g_server->setTargetWorld(gt::target_world_name);
                        }
                        bool hasAdmin = find(g_server->adminNetID.begin(), g_server->adminNetID.end(), netid) != g_server->adminNetID.end();
                        if (hasAdmin && gt::is_spam_active) {
                            g_server->adminNetID.erase(remove(g_server->adminNetID.begin(),
                                                              g_server->adminNetID.end(),
                                                              netid),
                                                       g_server->adminNetID.end());
                            gt::is_admin_entered = !g_server->adminNetID.empty();
                        }
                        players.erase(remove(players.begin(), players.end(), player), players.end());
                        break;
                    }
                }
            }
        }
            break;
        case fnv32("OnSpawn"):
        {
            string rawPacket = varlist.get(1).get_string();
            rtvar var = rtvar::parse(rawPacket);
            auto name = var.find("name");
            auto netID = var.find("netID");
            auto onlineID = var.find("onlineID");
            if (name && netID && onlineID)
            {
                Player ply{};
                bool isOwner = utils::toUpper(gt::owner_name) == utils::toUpper(utils::stripMessage(name->m_value));
                
                if (isOwner) gt::owner_net_id = var.get_int("netID");
                if (!gt::is_following_closest_player && gt::is_following_public) g_server->setTargetWorld(gt::target_world_name);
                gt::public_net_id = var.get_int("netID");
                
                if (!isOwner && (name->m_value.find("`2") != -1 || name->m_value.find("`^") != -1)) {
                    g_server->adminNetID.push_back(var.get_int("netID"));
                    gt::is_admin_entered = true;
                } else if (!isOwner && gt::is_auto_ban_joined) {
                    g_server->send("action|input\n|text|/ban " + utils::stripMessage(name->m_value));
                }
                
                g_server->playerName.push_back(utils::stripMessage(name->m_value));
                //            if (gt::is_activated_auto_message)
                //            {
                //                gt::is_auto_message_active = true;
                //                g_server->cv.notify_all();
                //            }
                
                if (var.find("invis")->m_value != "1")
                {
                    ply.name = utils::stripMessage(name->m_value);
                    ply.country = var.get("country");
                    name->m_values[0] += " `4[" + netID->m_value + "]``";
                    auto pos = var.find("posXY");
                    
                    if (pos && pos->m_values.size() >= 2)
                    {
                        auto x = atoi(pos->m_values[0].c_str());
                        auto y = atoi(pos->m_values[1].c_str());
                        ply.pos = vector2_t{float(x), float(y)};
                    }
                } else {
                    ply.mod = true;
                    ply.invis = true;
                    gt::is_exit = true;
                }
                if (var.get("mstate") == "1" || var.get("smstate") == "1") ply.mod = true;
                ply.userid = var.get_int("userID");
                ply.netid = var.get_int("netID");
                if (rawPacket.find("type|local") != -1)
                {
                    var.find("mstate")->m_values[0] = "1";
                    g_server->m_world.local = ply;
                }
                g_server->m_world.players.push_back(ply);
                auto str = var.serialize();
                utils::replace(str, "onlineID", "onlineID|");
                varlist[1] = str;
                // g_server->send(true, varlist, -1, -1);
                return true;
            }
        }
            break;
    }
    return false;
}

bool events::send::OnSendPing(gameupdatepacket_t *packet)
{
    //since this is a pointer we do not need to copy memory manually again
    packet->m_type = 21;        // ping packet
    packet->m_vec2_x = 1000.f;  // gravity
    packet->m_vec2_y = 250.f;   // move speed
    packet->m_vec_x = 64.f;     // punch range
    packet->m_vec_y = 64.f;     // build range
    packet->m_jump_amount = 0;  // for example unlim jumps set it to high which causes ban
    packet->m_player_flags = 0; // effect flags. good to have as 0 if using mod noclip, or etc.
    return false;
}

void events::send::OnLoginRequest()
{
    rtvar var = rtvar::parse("");
    auto hash_str = g_server->mac + "RT";
    var.append("tankIDName|" + gt::bot_name);
    var.append("tankIDPass|" + gt::bot_password);
    var.append("requestedName|JailBreak");
    var.append("f|0");
    var.append("protocol|158");
    var.append("game_version|" + gt::version);
    var.append("lmode|1");
    var.append("cbits|128");
    var.append("player_age|18");
    var.append("GDPR|2");
    var.append("tr|4322");
    var.append("meta|" + g_server->meta);
    var.append("fhash|-716928004");
    var.append("rid|" + utils::generateRid());
    var.append("platformID|2");
    var.append("deviceVersion|0");
    var.append("country|" + gt::flag);
    var.append("hash|" + to_string(utils::random(INT_MIN, INT_MAX)));
    var.append("mac|" + g_server->mac);
    var.append("wk|" + utils::generateRid());
    
    if (g_server->m_token != 0 && g_server->m_user != 0)
    {
        var.append("user|" + to_string(g_server->m_user));
        var.append("token|" + to_string(g_server->m_token));
        var.append("UUIDToken|" + g_server->m_uuid_token);
        var.append("doorID|" + g_server->m_doorID);
    }

    string packet = var.serialize();
    gt::in_game = false;
    g_server->send(packet);
}

void events::send::OnPlayerEnterGame()
{
    g_server->send("action|enter_game");
}

bool events::send::OnGenericText(string packet)
{
    //        PRINTS("Generic text: %s\n", packet.c_str());
    //        auto &world = g_server->m_world;
    //        rtvar var = rtvar::parse(packet);
    //        if (!var.valid())
    return false;
}

bool events::send::OnGameMessage(string packet)
{
    //     PRINTD("Game message: %s\n", packet.c_str());
    rtvar var = rtvar::parse(packet);
    string msg = var.get("msg");
    if (msg.find("password is wrong") != -1 || msg.find("suspended") != -1 || msg.find("this account is currently banned") != -1)
    {
        PRINTS("%s \n", utils::stripMessage(msg).c_str());
        g_server->quit();
        return true;
    } else if(msg.find("Server requesting that you re-logon") != -1 || msg.find("Too many people logging in at once") != -1)
    {
        g_server->reconnecting(true);
    }
    return false;
}

bool events::send::OnState(gameupdatepacket_t *packet)
{
    if (!g_server->m_world.connected)
        return false;
    if (!gt::owner_net_id) gt::owner_net_id = utils::getNetIDFromVector(&gt::owner_name);
    
    if (gt::is_following_owner && packet->m_player_flags == gt::owner_net_id)
    {
        g_server->m_world.local.pos = vector2_t{packet->m_vec_x, packet->m_vec_y};
        g_server->lastPos = vector2_t{packet->m_vec_x, packet->m_vec_y};
        g_server->m_world.local.packetFlag = packet->m_packet_flags;
        
        if (gt::is_following_punch && packet->m_state1 != -1 && packet->m_state2 != -1)
        {
            packet->m_type = PACKET_TILE_CHANGE_REQUEST;
            packet->m_int_data = 18;
        } else if (packet->m_state1 != -1 && packet->m_state2 != -1)
        {
            return true;
        }
        return false;
    }
    else if (gt::is_following_public && packet->m_state1 == -1 && packet->m_state2 == -1 && (packet->m_player_flags == gt::public_net_id ||
                                                                                             gt::is_following_closest_player))
    {
        return gt::is_following_closest_player ? !g_server->inRange(packet->m_vec_x, packet->m_vec_y, 80, 80) : false;
    }
    
    return true;
}

void events::send::OnSendTileActiveRequest(int posX, int posY) {
    gameupdatepacket_t packet{};
    packet.m_type = PACKET_TILE_ACTIVATE_REQUEST;
    packet.m_player_flags = g_server->m_world.local.netid;
    packet.m_vec_x = posX;
    packet.m_vec_y = posY;
    g_server->send(NET_MESSAGE_GAME_PACKET, (uint8_t*)&packet, sizeof(packet));
}

bool events::send::OnChangeObject(gameupdatepacket_t *packet) {
    if (packet->m_player_flags == -1) {
        g_server->playerInventory.increaseDroppedItemUID();
        if(gt::is_auto_collect &&
           packet->m_vec_x != 0 &&
           packet->m_vec_y != 0 &&
           packet->m_vec_y <= (g_server->lastPos.m_y - 30) &&
           packet->m_vec_y >= (g_server->lastPos.m_y - 75) &&
           packet->m_vec_x >= (g_server->lastPos.m_x - 80) &&
           packet->m_vec_x <= (g_server->lastPos.m_x + 80))
        {
            events::send::OnSendCollectDropItem(packet->m_vec_x, packet->m_vec_y);
            // TODO: - you need to handle increase of gems and level
            // if (packet->m_int_data == 112) g_server->gems = 0;
            if (packet->m_int_data != gt::block_id && packet->m_int_data != gt::block_id+1 && packet->m_int_data != 112) {
                g_server->send("action|trash\n|itemID|" + to_string(packet->m_int_data));
            }
        }
        if (gt::is_auto_drop && g_server->playerInventory.getTotalDroppedItem() >= gt::max_dropped_block) {
            g_server->send("action|drop\n|itemID|" + to_string(gt::block_id+1));
        }
    }
    return true;
}

bool events::send::OnSendMapData(gameupdatepacket_t *packet, long packetLength)
{
    g_server->m_world = {};
    auto extended = utils::get_extended(packet);
    extended += 4;
    auto data = extended + 6;
    auto name_length = *(short *)data;
    
    uint32_t totalTiles = *(uint32_t *)(extended + name_length + 16);
    g_server->m_world.posPtr = name_length + 20;
    
    char *name = new char[name_length + 1];
    memcpy(name, data + sizeof(short), name_length);
    char none = '\0';
    memcpy(name + name_length, &none, 1);
    /* *********************************************** */
    
    g_server->m_world.foreground = (__int16_t *)malloc(totalTiles * sizeof(__int16_t));
    g_server->m_world.background = (__int16_t *)malloc(totalTiles * sizeof(__int16_t));
    
    for (int i = 0; i < totalTiles; i++) {
        if(g_server->m_world.posPtr >= packetLength) break;
        g_server->m_world.tileSerialize(extended, i);
    }
    
    int itemUID = *(uint16_t*)(extended + g_server->m_world.posPtr + 20);
    if (itemUID >= 0) {
        g_server->playerInventory.setDroppedItemUID(itemUID);
    }
    /* *********************************************** */
    
    g_server->m_world.name = string(name);
    MenuBar::refreshStatusWindow(TYPE_WORLD);
    g_server->m_world.connected = true;
    if (string(name) != gt::target_world_name)
    {
        g_server->setTargetWorld(gt::target_world_name);
    }
    
    // we need to clean up all allocation memory after used
    delete[] name;
    free(g_server->m_world.foreground);
    free(g_server->m_world.background);
    return false;
}

void events::send::OnSendPacketMove(float posX, float posY, int characterState)
{
    gameupdatepacket_t packet{};
    packet.m_type = PACKET_STATE;
    packet.m_player_flags = g_server->m_world.local.netid;
    packet.m_packet_flags = characterState;
    packet.m_vec_x = posX;
    packet.m_vec_y = posY;
    g_server->send(NET_MESSAGE_GAME_PACKET, (uint8_t*)&packet, sizeof(packet));
}

bool events::send::OnSendCollectDropItem(float posX, float posY)
{
    gameupdatepacket_t packet{};
    packet.m_type = PACKET_ITEM_ACTIVATE_OBJECT_REQUEST;
    packet.m_player_flags = g_server->m_world.local.netid;
    packet.m_vec_x = posX;
    packet.m_vec_y = posY;
    packet.m_int_data = g_server->playerInventory.getDroppedItemUID();
    packet.m_state1 = posX + posY + 4;
    g_server->send(NET_MESSAGE_GAME_PACKET, (uint8_t*)&packet, sizeof(packet));
    return true;
}

bool events::send::OnSetTrackingPacket(string packet) {
    rtvar var = rtvar::parse(packet);
    string eventName = var.find("eventName")->m_value;
    
    if (eventName == "305_DONATIONBOX") {
        int itemID = var.get_int("item");
        if (gt::block_id == itemID) {
            g_server->playerInventory.setTotalInventoryBlock(var.get_int("itemamount"));
        } else {
            g_server->send("action|drop\n|itemID|" + to_string(itemID));
        }
    } else if (eventName == "305_DROP") {
        if (var.get_int("Item_id") == gt::block_id + 1) {
            g_server->playerInventory.setTotalDroppedItem(0);
        }
    } else if (eventName == "100_MOBILE.START") {
        g_server->gems = var.get_int("Gems_balance");
        g_server->level = var.get_int("Level");
    } else {
        // We don't need to handle all tracking packet
        events::send::OnPlayerEnterGame();
    }
    MenuBar::refreshStatusWindow(TYPE_BOTTOM);
    return true;
}

void events::send::OnSendChatPacket()
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lk(g_server->mtx);
            g_server->cv.wait(lk, [] { return gt::is_spam_active; });
        }
        if (gt::is_spam_active && g_server->m_world.connected && gt::safety_spam == 0 && !gt::is_admin_entered)
        {
            g_server->send("action|input\n|text|" + gt::spam_text);
        }
        if(gt::safety_spam != 0) gt::safety_spam--;
        this_thread::sleep_for(chrono::milliseconds(gt::spam_delay));
    }
}

void events::send::OnSendMessagePacket()
{
    /*
     * we need to order the player name from vector
     * and msg the player in the world every 3 seconds
     * TODO: - this function may have a bug and you need to debug this in the future
     * TODO: - auto message this currently won't work because APP security
     */
    while (true)
    {
        {
            std::unique_lock<std::mutex> lk(g_server->mtx);
            g_server->cv.wait(lk, [] { return gt::is_auto_message_active; });
        }
        if (gt::is_auto_message_active && !g_server->playerName.empty() && g_server->m_world.connected)
        {
            g_server->send("action|input\n|text|/msg " +
                           g_server->playerName[g_server->playerName.size() - 1] + " "
                           + utils::generateQuotes(utils::colorStr(gt::spam_text)));
            g_server->playerName.pop_back();
        }
        //        else
        //        {
        //            gt::is_auto_message_active = false;
        //            g_server->cv.notify_one();
        //        }
        this_thread::sleep_for(chrono::milliseconds(5000));
    }
}

void events::send::OnSendTileChangeRequestPacket()
{
    float *x       = &g_server->lastPos.m_x;
    float *y       = &g_server->lastPos.m_y;
    bool isBreak   = true;
    gameupdatepacket_t packet{};
    packet.m_type = PACKET_TILE_CHANGE_REQUEST;
    
    while (true)
    {
        {
            std::unique_lock<std::mutex> lk(g_server->mtx);
            g_server->cv.wait(lk, [] { return gt::is_use_tile; });
        }
        
        if (gt::is_use_tile && g_server->m_world.connected && (gt::is_auto_break_active || gt::is_auto_place_active))
        {
            if (gt::is_auto_break_active && gt::is_auto_place_active) {
                isBreak = !isBreak;
            } else if (gt::is_auto_break_active) {
                isBreak = true;
            } else if (gt::is_auto_place_active) {
                isBreak = false;
            }
            
            packet.m_vec_x = *x;
            packet.m_vec_y = *y;
            packet.m_state1 = (int)(*x / 32) - 3;
            packet.m_state2 = (int)ceil(*y / 32) - 3;
            packet.m_int_data = isBreak ? 18 : gt::block_id;
            for (int i = 1; i <= 5; i++)
            {
                packet.m_state1++;
                for(int counterHit = 1; counterHit <= (isBreak ? gt::hit_per_block : 2); counterHit++) {
                    g_server->send(NET_MESSAGE_GAME_PACKET, (uint8_t*)&packet, sizeof(packet));
                    this_thread::sleep_for(chrono::milliseconds(isBreak ? 180 : 110));
                }
            }
            
            if (gt::is_auto_place_active && g_server->playerInventory.getTotalCurrentBlock() <= 0) {
                this_thread::sleep_for(chrono::milliseconds(500)); // We need to put delay here to make sure all block was breaked!
                packet.m_state1 = ((int)(*x / 32) - 2) + g_server->donationBoxPosition;
                packet.m_state2 = (int)ceil(*y / 32) - 2;
                packet.m_int_data = 32;
                if (g_server->playerInventory.getTotalCurrentBlock() <= 0)
                    g_server->send(NET_MESSAGE_GAME_PACKET, (uint8_t*)&packet, sizeof(packet));
            }
        } else {
            this_thread::sleep_for(chrono::seconds(3));
        }
    }
}

