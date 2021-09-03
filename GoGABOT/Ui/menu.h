#pragma once

#include <ncurses.h>
#include <iostream>
#include <string>
#include "../utils.h"
#include "../gt.hpp"
#include "../events.h"
#include "../server.h"

using namespace std;

#ifndef _MENU_H_
#define _MENU_H_

enum MenuType
{
    TYPE_NUMBER,
    TYPE_SELECTED,
    TYPE_DEFAULT,
};

class Menu
{
public:
    int start_y;
    int increaseBy;
    int typeMenu;
    int currentValue;
    int minValue;
    int maxValue;
    bool enable;
    string text;
    
    Menu(string text, int currentValue = 0, int increaseBy = 1, int maxValue = 0, int menuType = TYPE_SELECTED)
    {
        this->text = text;
        this->enable = false;
        this->increaseBy = increaseBy;
        this->typeMenu = menuType;
        this->currentValue = currentValue;
        this->minValue = currentValue;
        this->maxValue = maxValue;
    }
};

class MenuBar
{
private:
    WINDOW *win;
    Menu *menus;
    int num_menus;
    int selected_menu;
    int maxX;
    
    void selectedType(int *type, int *start_y, int *currentValue, bool *isEnable)
    {
        switch (*type)
        {
            case TYPE_NUMBER:
                mvwprintw(win, *start_y, maxX, ("< " + to_string(*currentValue) + " >").c_str());
                
                break;
            default:
                mvwprintw(win, *start_y, maxX, *isEnable ? "< ON >" : "< OFF >");
                break;
        }
    }
    
    void drawActionMenu()
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
                this->selectedType(&menuType, &start_y, &currentValue, &isEnable);
                wattroff(win, A_REVERSE);
            }
            else
            {
                this->selectedType(&menuType, &start_y, &currentValue, &isEnable);
            }
        }
        box(this->win, 0, 0);
    }
    
    void handleActionType(bool isIncrement = false)
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
    
public:
    MenuBar(WINDOW *win, Menu *menus, int num_menus, int maxX)
    {
        this->win = win;
        this->menus = menus;
        this->num_menus = num_menus;
        this->maxX = maxX;
        this->selected_menu = 0;
        
        for (int i = 0; i < num_menus; i++)
        {
            this->menus[i].start_y = i + 1;
        }
    }
    
    void drawMenu()
    {
        for (int i = 0; i < num_menus; i++)
        {
            int start_y = this->menus[i].start_y;
            string text = this->menus[i].text;
            mvwprintw(win, start_y, 1, text.c_str());
        }
        this->drawActionMenu();
    }
    
    void handleTrigger(int trigger)
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
    
    void drawStatusMenu(WINDOW *winOrigin, string connectionStatus, string currentWorld, string botStatus)
    {
        // TODO: - still have issue with character render
        mvwprintw(winOrigin, 12, 1, "[-] PING: 30ms");
        mvwprintw(winOrigin, 13, 1, "[-] SERVER CONNECTION: %s", connectionStatus.c_str());
        mvwprintw(winOrigin, 14, 1, "[-] CURRENT WORLD:", currentWorld.c_str());
        mvwprintw(winOrigin, 15, 1, "[-] BOT NAME: %s", utils::toUpper(gt::bot_name).c_str());
        mvwprintw(winOrigin, 16, 1, "[-] BOT STATUS: %s", botStatus.c_str());
        wattron(winOrigin, A_REVERSE);
        mvwprintw(winOrigin, 17, 1, "OPTION KEY: [ON] O, [OFF] F");
        mvwprintw(winOrigin, 18, 1, "[UP] W, [DOWN] S, [+] P, [-] L");
        wattroff(winOrigin, A_REVERSE);
        wrefresh(winOrigin);
    }
    
    void bindingKey()
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
                    gt::max_dropped_block = valueOfActiveMenu;
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
};

#endif
