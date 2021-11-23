#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <string>
#include <fstream>
#include "enet/include/enet.h"
#include "server.h"
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
        gt::block_id = getUserInfo["block_id"];
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
    cin >> gt::block_id;
    
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
        // we don't need to store information
        // utils::saveUserInfo(gt::bot_name, gt::bot_password, gt::owner_name, gt::block_id);
    }
    
    initscr();
    noecho();
    curs_set(0);
    enet_initialize();
    g_server->mac = utils::generateMac();
    g_server->m_world.setupItemDefs();
    if (g_server->connect(true))
    {
        int screenMaxY, screenMaxX;
        getmaxyx(stdscr, screenMaxY, screenMaxX);
        string botStatus = "OFFLINE";

        WINDOW *creditWindow = newwin(5, 36, 2, 4);
        box(creditWindow, 0, 0);
        mvwprintw(creditWindow, 1, 1, ("---=====[ GoGABOT "+gt::client_version+" ]=====---").c_str());
        mvwprintw(creditWindow, 2, 4, "BOT made bY Johny ft Github");
        mvwprintw(creditWindow, 3, 9, "GoGABOT (c) 2021");
        wrefresh(creditWindow);
        WINDOW *win = newwin(22, 36, 7, 4);
        box(win, 0, 0);
        keypad(win, TRUE);
        
        MenuStruct menus[] = {
            MenuStruct("[+] SPAM"),
            MenuStruct("[+] SPAM DELAY", 3500, 100, 3500 * 2,TYPE_NUMBER),
            MenuStruct("[+] AUTO MESSAGE"),
            MenuStruct("[+] FOLLOWING OWNER"),
            MenuStruct("[+] FOLLOWING PUBLIC"),
            MenuStruct("[+] FOLLOWING THE CLOSEST"),
            MenuStruct("[+] FOLLOWING PUNCH"),
            MenuStruct("[+] AUTO BAN SELLER"),
            MenuStruct("[+] AUTO BAN JOINED"),
            MenuStruct("[+] AUTO COLLECT"),
            MenuStruct("[+] AUTO DROP", 0, 50, 150, TYPE_NUMBER),
            MenuStruct("[+] AUTO BREAK ON TOP"),
            MenuStruct("[+] HIT PER BLOCK", 1, 1, 20, TYPE_NUMBER),
            MenuStruct("[+] AUTO PLACE"),
            MenuStruct("[+] EXIT", TYPE_DEFAULT),
        };

        MenuBar menuBar = MenuBar(win, menus, sizeof(menus) / sizeof(menus[0]), 27);
        menuBar.win = win;
        menuBar.drawMenu();
        
        MenuBar::refreshStatusWindow(TYPE_NAME);
        MenuBar::refreshStatusWindow(TYPE_WORLD);
        MenuBar::refreshStatusWindow(TYPE_BOTTOM);
        
        std::thread binKey(&MenuBar::bindingKey, &menuBar);
        std::thread spamChat(events::send::OnSendChatPacket);
        std::thread autoMsg(events::send::OnSendMessagePacket);
        std::thread autoBreak(events::send::OnSendTileChangeRequestPacket);
        if (spamChat.joinable() &&
            autoMsg.joinable() &&
            binKey.joinable() &&
            autoBreak.joinable())
        {
            spamChat.detach();
            autoMsg.detach();
            binKey.detach();
            autoBreak.detach();
        }
        
        while (!gt::is_exit)
        {
            g_server->eventHandle();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } else {
        endwin();
        cout << "Error when trying start client." << endl;
        return 0;
    }
    
    endwin();
    return 0;
}
