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

using namespace std;

bool events::send::variantList(gameupdatepacket_t *packet)
{
    variantlist_t varlist{};
    auto extended = utils::get_extended(packet);
    extended += 4; //since it casts to data size not data but too lazy to fix this
    varlist.serialize_from_mem(extended);
    // PRINTD("varlist: %s\n", varlist.print().c_str());
    auto func = varlist[0].get_string();

    // probably subject to change, so not including in switch statement.
    if (func.find("OnSuperMainStartAcceptLogon") != -1)
        gt::in_game = true;

    switch (hs::hash32(func.c_str()))
    {
    // solve captcha
    case fnv32("onShowCaptcha"):
    {
        auto menu = varlist[1].get_string();
        gt::solve_captcha(menu);
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
        if (stripText.find("[W]_") == -1)
            return true;

        string playerName = utils::getValueFromPattern(text, "`6<`w(.*)``>``");
        if (gt::is_auto_ban && !playerName.empty() && (utils::toUpper(stripText).find("SELL") != -1 || utils::toUpper(stripText).find("GO ") != -1 || utils::toUpper(stripText).find("SCAM") != -1 || utils::toUpper(stripText).find("GOTO ") != -1 || utils::toUpper(stripText).find("SKEM") != -1))
        {
            g_server->send("action|input\n|text|/ban " + playerName);
        }
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
            }
            if (content.find("add_label_with_icon|big|`wThe Growtopia Gazette") != -1)
            {
                g_server->send("action|dialog_return\ndialog_name|gazette");
                return true;
            }
        }
        if (false)
        { // TODO: this will be used in the future
            string itemid = content.substr(content.find("embed_data|itemID|") + 18, content.length() - content.find("embed_data|itemID|") - 1);
            string count = content.substr(content.find("count||") + 7, content.length() - content.find("count||") - 1);
            if (content.find("embed_data|itemID|") != -1)
            {
                if (content.find("Drop") != -1)
                {
                    g_server->send("action|dialog_return\ndialog_name|drop_item\nitemID|" + itemid + "|\ncount|" + count);
                    return true;
                }
            }
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
                    players.erase(remove(players.begin(), players.end(), player), players.end());
                    break;
                }
            }
        }
    }
    break;
    case fnv32("OnSpawn"):
    {
        string rawData = varlist.get(1).get_string();
        rtvar var = rtvar::parse(rawData);
        auto name = var.find("name");
        auto netID = var.find("netID");
        auto onlineID = var.find("onlineID");
        if (name && netID && onlineID)
        {
            player ply{};

            if (utils::toUpper(gt::owner_name) == utils::toUpper(utils::stripMessage(name->m_value)))
            {
                gt::owner_net_id = var.get_int("netID");
            }

            g_server->playerName.push_back(utils::stripMessage(name->m_value));
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
            }
            else
            {
                ply.mod = true;
                ply.invis = true;
            }
            if (var.get("mstate") == "1" || var.get("smstate") == "1")
                ply.mod = true;
            ply.userid = var.get_int("userID");
            ply.netid = var.get_int("netID");
            if (rawData.find("type|local") != -1)
            {
                var.find("mstate")->m_values[0] = "1";
                g_server->m_world.local = ply;
            }
            g_server->m_world.players.push_back(ply);
            auto str = var.serialize();
            utils::replace(str, "onlineID", "onlineID|");
            varlist[1] = str;
            // PRINTD("new: %s\n", varlist.print().c_str());
            // g_server->send(true, varlist, -1, -1);
            return true;
        }
    }
    break;
    }
    return false;
}

bool events::send::onPingReply(gameupdatepacket_t *packet)
{
    //since this is a pointer we do not need to copy memory manually again
    packet->m_type = 21;        // ping packet
    packet->m_vec2_x = 1000.f;  //gravity
    packet->m_vec2_y = 250.f;   //move speed
    packet->m_vec_x = 64.f;     //punch range
    packet->m_vec_y = 64.f;     //build range
    packet->m_jump_amount = 0;  //for example unlim jumps set it to high which causes ban
    packet->m_player_flags = 0; //effect flags. good to have as 0 if using mod noclip, or etc.
    return false;
}

void events::send::onLoginRequest()
{
    rtvar var = rtvar::parse("");
    auto mac = utils::generateMac();
    auto hash_str = mac + "RT";
    auto hash2 = utils::hash((uint8_t *)hash_str.c_str(), hash_str.length());
    var.append("tankIDName|" + gt::bot_name);
    var.append("tankIDPass|" + gt::bot_password);
    var.append("requestedName|JailBreak");
    var.append("f|0");
    var.append("protocol|133");
    var.append("game_version|" + gt::version);
    var.append("lmode|1");
    var.append("cbits|128");
    var.append("player_age|18");
    var.append("GDPR|2");
    var.append("tr|4322");
    var.append("meta|" + utils::random(utils::random(6, 10)) + ".com");
    var.append("fhash|-716928004");
    var.append("rid|" + utils::generateRid());
    var.append("platformID|4");
    var.append("deviceVersion|0");
    var.append("country|" + gt::flag);
    var.append("hash|" + to_string(utils::random(INT_MIN, INT_MAX)));
    var.append("mac|" + mac);
    var.append("wk|" + utils::generateRid());

    if (g_server->m_token != 0 && g_server->m_user != 0)
    {
        var.append("user|" + to_string(g_server->m_user));
        var.append("token|" + to_string(g_server->m_token));
        /*
        *
        * we will need used it in the future to handle a redirect subserver when player using a doorID
        * if(!g_server->m_doorID.empty()) var.append("doorID|" + g_server->m_doorID);
        * 
        */
    }

    string packet = var.serialize();
    gt::in_game = false;
    // PRINTS("Spoofing login info: \n");
    g_server->send(packet);
}

void events::send::onPlayerEnterGame()
{
    rtvar var = rtvar::parse("");
    var.append("action|enter_game");
    string packet = var.serialize();
    g_server->send(packet);
}

bool events::send::onGenericText(string packet)
{
    // PRINTS("Generic text: %s\n", packet.c_str());
    auto &world = g_server->m_world;
    rtvar var = rtvar::parse(packet);
    if (!var.valid())
        return false;
}

bool events::send::onGameMessage(string packet)
{
    // PRINTS("Game message: %s\n", packet.c_str());
    if (packet == "action|quit")
    {
        g_server->quit();
        return true;
    }

    return false;
}

bool events::send::onState(gameupdatepacket_t *packet)
{
    if (!g_server->m_world.connected)
        return false;
    g_server->m_world.local.pos = vector2_t{packet->m_vec_x, packet->m_vec_y};

    if (!gt::owner_net_id)
    {
        gt::owner_net_id = utils::getNetIDFromVector(&gt::owner_name);
    }
    if (gt::is_following_owner && packet->m_player_flags == gt::owner_net_id)
    {
        if (gt::is_following_punch && packet->m_state1 != -1 && packet->m_state2 != -1)
        {
            packet->m_type = PACKET_TILE_CHANGE_REQUEST;
            packet->m_int_data = 18;
            return false;
        }
        else if (packet->m_state1 == -1 && packet->m_state2 == -1)
        {
            return false;
        }

        return true;
    }
    else if (gt::is_following_public && packet->m_state1 == -1 && packet->m_state2 == -1)
    {
        return false;
    }

    return true;
}

bool events::send::onSendMapData(gameupdatepacket_t *packet)
{
    g_server->m_world = {};
    auto extended = utils::get_extended(packet);
    extended += 4;
    auto data = extended + 6;
    auto name_length = *(short *)data;

    char *name = new char[name_length + 1];
    memcpy(name, data + sizeof(short), name_length);
    char none = '\0';
    memcpy(name + name_length, &none, 1);

    g_server->m_world.name = string(name);
    g_server->m_world.connected = true;
    delete[] name;

    if (string(name) != gt::target_world_name)
    {
        g_server->setTargetWorld(gt::target_world_name);
    }
    return false;
}

void events::send::onSendChatPacket()
{

    /*
    * This is a thread don't put anything code
    * under this function if you don't understand about a thread!!
    * because can make a thread race!!
    * */

    while (true)
    {
        {
            /*
            * As far as I know this used for thread management
            * this thread will wait until global variable active
            * if you have any idea feel free to comment about this
            */
            std::unique_lock<std::mutex> lk(g_server->mtx);
            g_server->cv.wait(lk, [] { return gt::is_spam_active; });
            /* 
            * [UPDATE] - I have no idea about it this will be TODO in the future
            */
        }
        if (gt::is_spam_active && g_server->m_world.connected)
        {
            g_server->send("action|input\n|text|" + gt::spam_text);
        }
        this_thread::sleep_for(chrono::milliseconds(gt::spam_delay));
    }
}

void events::send::onSendMessagePacket()
{
    /*
    * we need to order the player name from vector
    * and msg the player in the world every 3 seconds
    * TODO: - this function may have a bug and you need to debug this in the future
    */
    int player_name_position = 0x0;

    while (true)
    {
        {
            std::unique_lock<std::mutex> lk(g_server->mtx);
            g_server->cv.wait(lk, [] { return gt::is_auto_message_active; });
        }
        if (gt::is_auto_message_active && !g_server->playerName.empty())
        {
            g_server->send("action|input\n|text|/msg " + g_server->playerName.at(player_name_position) + " " + gt::spam_text);
            g_server->playerName.erase(g_server->playerName.begin());
            player_name_position++;
        }
        else
        {
            g_server->playerName.clear();
            player_name_position = 0x0;
        }
        this_thread::sleep_for(chrono::milliseconds(3000));
    }
}

void events::send::onSendPunchPacket()
{
    uint32_t data_size = 0;
    int32_t *netID = &g_server->m_world.local.netid;
    float *x = &g_server->m_world.local.pos.m_x;
    float *y = &g_server->m_world.local.pos.m_y;

    while (true)
    {
        {
            std::unique_lock<std::mutex> lk(g_server->mtx);
            g_server->cv.wait(lk, [] { return gt::is_auto_break_active; });
        }
        if (gt::is_auto_break_active)
        {
            auto update_packet = MALLOC(gameupdatepacket_t, +data_size);
            memset(update_packet, 0, sizeof(gameupdatepacket_t) + data_size);
            update_packet->m_type = PACKET_TILE_CHANGE_REQUEST;
            update_packet->m_player_flags = *netID;
            update_packet->m_packet_flags |= 8;
            update_packet->m_vec_x = *x;
            update_packet->m_vec_y = *y;
            update_packet->m_state2 = ceil(*y / 32) - 3;
            update_packet->m_int_data = 18;
            update_packet->m_data_size = data_size;
            for (int i = -3; i <= 1; i++)
            {
                update_packet->m_state1 = ceil(*x / 32) + i;
                g_server->send(NET_MESSAGE_GAME_PACKET, update_packet, 56);
                this_thread::sleep_for(chrono::milliseconds(150));
            }
            free(update_packet);
        }
    }
}