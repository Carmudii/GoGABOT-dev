#pragma once

#include <string>
#include <vector>
#include "player.h"
#include "utils.h"

using namespace std;

struct ItemDefinitionStruct
{
    int id;
    int actionType;
    string itemName;
};

class World {
public:
    static vector<ItemDefinitionStruct> itemDefs;
    static __int16_t *foreground;
    static __int16_t *background;
    static long posPtr;
    ItemDefinitionStruct GetItemDef(int itemID);

    string name{};
    vector<Player> players{};
    Player local{};
    bool connected{};
    bool requiresTileExtra(int id);
    void setupItemDefs();
    void tileExtraSerialize(uint8_t *extended, int location);
    void tileSerialize(uint8_t *extended, int location);
};
