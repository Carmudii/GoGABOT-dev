//
//  playerInventory.h
//  GoGABOT
//
//  Created by Carmudi on 02/09/21.
//  Copyright © 2021 9GATE. All rights reserved.
//

#include <thread>
#include "menu.h"

using namespace std;

WINDOW *MenuBar::win;

void MenuBar::selectedType(int &type, int &start_y, int &currentValue, bool &isEnable)
{
    switch (type)
    {
        case TYPE_NUMBER:
            mvwprintw(win, start_y, maxX, ("> " + to_string(currentValue)).c_str());
            break;
        default:
            mvwprintw(win, start_y, maxX, isEnable ? "[ ON  ]" : "[ OFF ]");
            break;
    }
}

void MenuBar::drawActionMenu()
{
    for (int i = 0; i < num_menus; i++)
    {
        int start_y = this->menus[i].start_y;
        int menuType = this->menus[i].typeMenu;
        int currentValue = this->menus[i].currentValue;
        bool isEnable = this->menus[i].enable;
        
        wclrtoeol(this->win);
        if (selected_menu == i)
        {
            wattron(win, A_REVERSE);
            this->selectedType(menuType, start_y, currentValue, isEnable);
            wattroff(win, A_REVERSE);
        }
        else
        {
            this->selectedType(menuType, start_y, currentValue, isEnable);
        }
    }
    box(this->win, 0, 0);
}

void MenuBar::handleActionType(bool isIncrement)
{
    bool currentStatus = this->menus[selected_menu].enable;
    int currentValue = this->menus[selected_menu].currentValue;
    int increaseBy = this->menus[selected_menu].increaseBy;
    int minValue = this->menus[selected_menu].minValue;
    int maxValue = this->menus[selected_menu].maxValue;

    if (this->menus[selected_menu].typeMenu == TYPE_NUMBER)
    {
        if (isIncrement)
        {
            this->menus[selected_menu].currentValue += (currentValue == maxValue) ? 0 : increaseBy;
        }
        else
        {
            this->menus[selected_menu].currentValue -= (currentValue == minValue) ? 0 : increaseBy;
        }
    }
    else
    {
        this->menus[selected_menu].enable = !currentStatus;
    }
}

void MenuBar::drawMenu()
{
    for (int i = 0; i < num_menus; i++)
    {
        int start_y = this->menus[i].start_y;
        string text = this->menus[i].text;
        mvwprintw(win, start_y, 1, text.c_str());
    }
    this->drawActionMenu();
}

void MenuBar::handleTrigger(int trigger)
{
    switch (trigger)
    {
        case KEY_UP:
            if (selected_menu == 0)
            {
                selected_menu = num_menus - 1;
            }
            else
            {
                selected_menu -= 1;
            }
            break;
        case KEY_DOWN:
            if (selected_menu == (num_menus - 1))
            {
                selected_menu = 0;
            }
            else
            {
                selected_menu += 1;
            }
            break;
        case KEY_RIGHT:
            this->handleActionType(true);
            break;
        case KEY_LEFT:
            this->handleActionType();
            break;
        default:
            break;
    }
}

void MenuBar::refreshStatusWindow(WindowRefreshType type)
{
    string whiteSpace = string(34, ' ');
    
    switch (type) {
        case TYPE_WORLD:
            mvwprintw(win, 18, 1, whiteSpace.c_str());
            mvwprintw(win, 18, 1, "[=] WORLD    : %s", g_server->m_world.name.c_str());
            break;
        case TYPE_NAME:
            mvwprintw(win, 17, 1, whiteSpace.c_str());
            mvwprintw(win, 17, 1, "[=] BOT NAME : %s", utils::toUpper(gt::bot_name).c_str());
            break;
        case TYPE_BOTTOM:
            wattron(win, A_REVERSE);
            mvwprintw(win, 19, 1, whiteSpace.c_str());
            mvwprintw(win, 19, 1, "[%s], [BLOCK] %d, [SEED] %d",
                      g_server->getServerStatus().c_str(),
                      g_server->playerInventory.getTotalCurrentBlock(),
                      g_server->playerInventory.getTotalDroppedItem());
            wattroff(win, A_REVERSE);
            break;
        default:
            break;
    }
    wrefresh(win);
}

void MenuBar::bindingKey()
{
    while (1)
    {
        this->handleTrigger(wgetch(this->win));
        this->drawMenu();
        
        int indexOfMenu = this->selected_menu + 1;
        int valueOfActiveMenu = this->menus[selected_menu].currentValue;
        bool isActiveMenu = this->menus[selected_menu].enable;
        
        switch (indexOfMenu)
        {
            case 1:
                gt::is_spam_active = isActiveMenu;
                if (isActiveMenu) {
                    g_server->adminNetID.clear();
                    gt::is_admin_entered = false;
                }
                break;
            case 2:
                gt::spam_delay = valueOfActiveMenu;
                break;
            case 3:
                gt::is_auto_message_active = isActiveMenu;
                gt::is_activated_auto_message = isActiveMenu;
                break;
            case 4:
                gt::is_following_owner = isActiveMenu;
                break;
            case 5:
                gt::is_following_public = isActiveMenu;
                break;
            case 6:
                gt::is_following_closest_player = isActiveMenu;
                break;
            case 7:
                gt::is_following_punch = isActiveMenu;
                break;
            case 8:
                gt::is_auto_ban = isActiveMenu;
                break;
            case 9:
                gt::is_auto_ban_joined = isActiveMenu;
                break;
            case 10:
                gt::is_auto_collect = isActiveMenu;
                break;
            case 11:
                gt::is_auto_drop = valueOfActiveMenu > 0;
                gt::max_dropped_block = valueOfActiveMenu - 1;
                break;
            case 12:
                gt::is_auto_break_active = isActiveMenu;
                gt::is_use_tile = isActiveMenu ? isActiveMenu : gt::is_auto_place_active;
                break;
            case 13:
                gt::hit_per_block = valueOfActiveMenu;
                break;
            case 14:
                gt::is_auto_place_active = isActiveMenu;
                gt::is_use_tile = isActiveMenu ? isActiveMenu : gt::is_auto_break_active;
                break;
            default:
                gt::is_exit = isActiveMenu;
                break;
        }
        g_server->cv.notify_all();
    }
}
