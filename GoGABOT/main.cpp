#include <ncurses.h>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <string>
#include <fstream>
#include "enet/include/enet.h"
#include "server.h"
#include "Ui/menu.h"
#include "gt.hpp"
#include "utils.h"
#include "json.hpp"

using json = nlohmann::json;

server *g_server = new server();

int main()
{
#ifdef _WIN32
    SetConsoleTitleA("GoGABOT");
#endif
    
    string correctlyInput;
    json getUserInfo;
    bool isConfigurationFileExists = utils::ifExists(gt::configuration_file_name);
retype:
    
    system("clear");
    cout << R"(
_________       ________________ ________ _______ ________
 __  ____/______ __  ____/___    |___  __ )__  __ \___  __/
   _/ __  _  __ \_  / __  __  /| |__  __  |_  / / /__  /
 / /_/ /  / /_/ // /_/ /  _  ___ |_  /_/ / / /_/ / _  /
 \____/   \____/ \____/   /_/  |_|/_____/  \____/  /_/
---------------- Growtopia version v)" +
    gt::version + R"( ----------------
============ [LOGIN INTO YOUR BOT ACCOUNT] ============
)";
    
    if (!isConfigurationFileExists)
    {
        cout << "- [+] Enter owner name: ";
        cin >> gt::owner_name;
        cout << "- [+] Enter username: ";
        cin >> gt::bot_name;
        cout << "- [+] Enter password: ";
        cin >> gt::bot_password;
    }
    else
    {
        ifstream file(gt::configuration_file_name);
        file >> getUserInfo;
        gt::bot_name = getUserInfo["username"];
        gt::bot_password = getUserInfo["password"];
        gt::owner_name = getUserInfo["owner_name"];
        file.close();
    }
    cout << "- [+] Enter target world: ";
    cin >> gt::target_world_name;
    cout << "- [+] Leave it blank if you won't spam" << endl;
    cout << "- [+] Enter spam text: ";
    cin.ignore();
    getline(cin, gt::spam_text);
    cout << "- [+] Leave it blank if you won't break" << endl;
    cout << "- [+] Enter block id: ";
//    cin.ignore();
//    string bid;
//    getline(cin, bid);
    gt::block_id = 2; // atoi(bid.c_str());
    
    cout << R"(
============= [ YOUR BOT ACCOUNT DETAIL ] =============
- [+] Owner name   : )" +
    gt::owner_name + R"(
- [+] Target world : )" +
    gt::target_world_name + R"(
- [+] BOT username : )" +
    gt::bot_name + R"(
- [+] BOT password : )" +
    gt::bot_password + R"(
- [+] Spam text    : )" +
    gt::spam_text + R"(
- [+] Block Id     : )" +
    to_string(gt::block_id) + R"(
============= [ YOUR BOT ACCOUNT DETAIL ] =============
)";
    
badInput:
    
    cout << "- [+] Check your data when is right? [Y/n]: ";
    cin >> correctlyInput;
    //    correctlyInput = "y";
    
    if (utils::toUpper(correctlyInput) == "N")
    {
        goto retype;
    }
    else if (utils::toUpper(correctlyInput) != "Y")
    {
        cout << "- [+] Please type [Y/n] \n";
        goto badInput;
    }
    if (!isConfigurationFileExists)
    {
        utils::saveUserInfo(gt::bot_name, gt::bot_password, gt::owner_name);
    }
    
    system("clear");
    initscr();
    noecho();
    curs_set(0);
    enet_initialize();
    
    g_server->m_world.setupItemDefs();
    if (g_server->connect())
    {
        int screenMaxY, screenMaxX;
        getmaxyx(stdscr, screenMaxY, screenMaxX);
        string botStatus = "OFFLINE";
        
        WINDOW *creditWindow = newwin(5, 36, 2, 4);
        box(creditWindow, 0, 0);
        mvwprintw(creditWindow, 1, 1, "---=====[ GoGABOT v1.0.0 ]=====---");
        mvwprintw(creditWindow, 2, 4, "BOT made bY 9GATE-Comunity");
        mvwprintw(creditWindow, 3, 9, "GoGABOT (c) 2021");
        wrefresh(creditWindow);
        WINDOW *win = newwin(20, 36, 7, 4);
        WINDOW *statusWindow = newwin(20, 36, 22, 4);
        box(win, 0, 0);
        box(statusWindow, 0, 0);
        keypad(win, TRUE);
        
        Menu menus[] = {
            Menu("[+] SPAM"),
            Menu("[+] SPAM DELAY", 3500, 100, 3500 * 2,TYPE_NUMBER),
            Menu("[+] AUTO MESSAGE"),
            Menu("[+] FOLLOWING OWNER"),
            Menu("[+] FOLLOWING PUBLIC"),
            Menu("[+] FOLLOWING PUNCH"),
            Menu("[+] AUTO BAN PEOPLE"),
            Menu("[+] AUTO COLLECT"),
            Menu("[+] AUTO DROP", 0, 50, 150, TYPE_NUMBER),
            Menu("[+] AUTO BREAK ON TOP"),
            Menu("[+] HIT PER BLOCK", 1, 1, 20, TYPE_NUMBER),
            Menu("[+] AUTO PLACE"),
            Menu("[+] EXIT", TYPE_DEFAULT),
        };
        
        MenuBar menuBar = MenuBar(win, menus, sizeof(menus) / sizeof(menus[0]), 25);
        menuBar.drawMenu();
        
        std::thread binKey(&MenuBar::bindingKey, &menuBar);
        std::thread spamChat(events::send::onSendChatPacket);
        std::thread autoMsg(events::send::onSendMessagePacket);
        std::thread autoBreak(events::send::onSendTileChangeRequestPacket);
        binKey.detach();
        spamChat.detach();
        autoMsg.detach();
        autoBreak.detach();
        
        while (!gt::is_exit)
        {
            g_server->eventHandle();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } else {
        cout << "Error when trying start client." << endl;
    }
    
    endwin();
    return 0;
}
