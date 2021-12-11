//
//  playerInventory.h
//  GoGABOT
//
//  Created by Carmudi on 02/09/21.
//  Copyright © 2021 9GATE. All rights reserved.
//

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

enum WindowRefreshType {
    TYPE_WORLD,
    TYPE_NAME,
    TYPE_BOTTOM,
};

struct MenuStruct
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
    
    MenuStruct(string text, int currentValue = 0, int increaseBy = 1, int maxValue = 0, int menuType = TYPE_SELECTED)
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
    MenuStruct *menus;
    int num_menus;
    int selected_menu;
    int maxX;
    
    void selectedType(int &type, int &start_y, int &currentValue, bool &isEnable);
    void drawActionMenu();
    void handleActionType(bool isIncrement = false);
    
public:
    MenuBar(WINDOW *win, MenuStruct *menus, int num_menus, int maxX)
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
    
    static void refreshStatusWindow(WindowRefreshType type);
    static WINDOW *win;
    
    void drawMenu();
    void handleTrigger(int trigger);
    void drawStatusMenu(WINDOW *winOrigin, string connectionStatus, string currentWorld, string botStatus);
    void bindingKey();
};

extern MenuBar *g_menuBar;

#endif
