#include "events.h"
#include "gt.hpp"
#include "proton/hash.hpp"
#include "proton/rtparam.hpp"
#include "proton/variant.hpp"
#include "server.h"
#include "utils.h"
#include <thread>
#include <limits.h>
#include <iostream>

using namespace std;

bool events::out::variantList(gameupdatepacket_t *packet)
{
    variantlist_t varlist{};
    auto extended = utils::get_extended(packet);
    extended += 4; //since it casts to data size not data but too lazy to fix this
    varlist.serialize_from_mem(extended);
    PRINTC("varlist: %s\n", varlist.print().c_str());
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
    }
    break;
    case fnv32("OnSendToServer"):
        g_server->redirect_server(varlist);
        return true;
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
        string meme = varlist.get(1).get_string();
        rtvar var = rtvar::parse(meme);
        auto name = var.find("name");
        auto netid = var.find("netID");
        auto onlineid = var.find("onlineID");

        if (name && netid && onlineid)
        {
            player ply{};

            if (var.find("invis")->m_value != "1")
            {
                ply.name = name->m_value;
                ply.country = var.get("country");
                name->m_values[0] += " `4[" + netid->m_value + "]``";
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
            if (meme.find("type|local") != -1)
            {
                var.find("mstate")->m_values[0] = "1";
                g_server->m_world.local = ply;
            }
            g_server->m_world.players.push_back(ply);
            auto str = var.serialize();
            utils::replace(str, "onlineID", "onlineID|");
            varlist[1] = str;
            PRINTC("new: %s\n", varlist.print().c_str());
            // g_server->send(true, varlist, -1, -1);
            return true;
        }
    }
    break;
    }
    return false;
}

bool events::out::pingReply(gameupdatepacket_t *packet)
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

void events::out::onLoginRequest(string username, string password)
{
    rtvar var = rtvar::parse("");
    auto mac = utils::generate_mac();
    auto hash_str = mac + "RT";
    auto hash2 = utils::hash((uint8_t *)hash_str.c_str(), hash_str.length());
    var.append("tankIDName|" + username);
    var.append("tankIDPass|" + password);
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
    var.append("rid|" + utils::generate_rid());
    var.append("platformID|4");
    var.append("deviceVersion|0");
    var.append("country|" + gt::flag);
    var.append("hash|" + to_string(utils::random(INT_MIN, INT_MAX)));
    var.append("mac|" + mac);
    var.append("wk|" + utils::generate_rid());

    if (g_server->m_token != 0 && g_server->m_user != 0)
    {
        var.append("user|" + to_string(g_server->m_user));
        var.append("token|" + to_string(g_server->m_token));
        /*
        *
        * we will need used it in the future to handle a redirect subserver when player using a doorID
        * if(!g_server->m_doorID.empty()) var.append("doorID|" + g_server->m_doorID);
        */
    }

    string packet = var.serialize();
    gt::in_game = false;
    PRINTS("Spoofing login info: \n");
    cout << packet << endl;
    g_server->send(packet);
}

void events::out::onPlayerEnterGame()
{
    rtvar var = rtvar::parse("");
    var.append("action|enter_game");
    string packet = var.serialize();
    cout << packet << endl;
    g_server->send(packet);
}

bool events::out::genericText(string packet)
{
    PRINTS("Generic text: %s\n", packet.c_str());
    auto &world = g_server->m_world;
    rtvar var = rtvar::parse(packet);
    if (!var.valid())
        return false;
    // if (packet.find("game_version|") != -1)
    // {
    //     rtvar var = rtvar::parse(packet);
    //     auto mac = utils::generate_mac();
    //     auto hash_str = mac + "RT";
    //     auto hash2 = utils::hash((uint8_t *)hash_str.c_str(), hash_str.length());
    //     var.set("mac", mac);
    //     var.set("wk", utils::generate_rid());
    //     var.set("rid", utils::generate_rid());
    //     var.set("fz", to_string(utils::random(INT_MIN, INT_MAX)));
    //     var.set("zf", to_string(utils::random(INT_MIN, INT_MAX)));
    //     var.set("hash", to_string(utils::random(INT_MIN, INT_MAX)));
    //     var.set("hash2", to_string(hash2));
    //     var.set("meta", utils::random(utils::random(6, 10)) + ".com");
    //     var.set("game_version", gt::version);
    //     var.set("country", gt::flag);

    //     /*
    //     AAP Bypass
    //     Only making this public because after 1 month being reported to ubi, nothing happened
    //     Then after a month (around 15.3) it got fixed for a whole single 1 day, and they publicly said it had been fixed
    //     And at that time we shared how to do it because thought its useless, and then aap bypass started working again
    //     and then 9999 new aap bypass services came to be public, and even playingo started selling it so no point keeping it private
    //     With publishing this I hope ubi actually does something this time
    //     */

    //     //Finally patched, I guess they finally managed to fix this after maybe a year!

    //     //if (var.find("tankIDName") && gt::aapbypass) {
    //     //    var.find("mac")->m_values[0] = "02:00:00:00:00:00";
    //     //    var.find("platformID")->m_values[0] = "4"; //android
    //     //    var.remove("fz");
    //     //    var.remove("rid");
    //     //}

    //     packet = var.serialize();
    //     gt::in_game = false;
    //     PRINTS("Spoofing login info\n");
    //     g_server->send(packet);
    //     return true;
    // }

    return false;
}

bool events::out::gameMessage(string packet)
{
    PRINTS("Game message: %s\n", packet.c_str());
    if (packet == "action|quit")
    {
        g_server->quit();
        return true;
    }

    return false;
}

bool events::out::state(gameupdatepacket_t *packet)
{
    if (!g_server->m_world.connected)
        return false;

    g_server->m_world.local.pos = vector2_t{packet->m_vec_x, packet->m_vec_y};
    PRINTS("local pos: %.0f %.0f\n", packet->m_vec_x, packet->m_vec_y);

    if (gt::ghost)
        return true;
    return false;
}