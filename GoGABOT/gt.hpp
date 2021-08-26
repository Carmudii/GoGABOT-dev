#pragma once

#include <string>

using namespace std;

namespace gt
{
    /************************** CONFIGURE BOT **************************/
    extern string target_world_name;
    extern string owner_name;
    extern string version;
    extern string bot_name;
    extern string bot_password;
    extern string spam_text;
    extern string flag;
    extern string configuration_file_name;
    extern string base_file_name;
    extern string item_defs_file_name;


    extern int owner_net_id;
    extern int safety_spam;
    /*********************** CONFIGURE MENU BOT ***********************/
    extern int spam_delay;
    extern int hit_per_block;
    extern int block_id;
    extern int max_dropped_block;
    /*********************** CONFIGURE MENU BOT ***********************/

    extern bool connecting;
    extern bool in_game;
    /*********************** CONFIGURE MENU BOT ***********************/
    extern bool is_spam_active;
    extern bool is_auto_message_active;
    extern bool is_use_tile;
    extern bool is_auto_break_active;
    extern bool is_auto_place_active;
    extern bool is_auto_collect;
    extern bool is_auto_drop;
    extern bool is_following_owner;
    extern bool is_following_public;
    extern bool is_following_punch;
    extern bool is_auto_ban;
    extern bool is_owner_command;
    extern bool is_exit;
    extern bool is_activated_auto_message;
    /************************** CONFIGURE BOT **************************/

    void solve_captcha(string text);
} // namespace gt
