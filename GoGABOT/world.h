#pragma once

#include <string>
#include <vector>
#include "player.h"
#include "utils.h"

using namespace std;

class World {
public:
    struct ItemDefinitionStruct
    {
        int id;
        int actionType;
        string itemName;
    };

    struct DroppedItemStruct
    {
        int posX;
        int posY;
        bool operator==(const DroppedItemStruct& item) {
            return posX == item.posX && posY == item.posY;
        }
    };
    
    static vector<World::ItemDefinitionStruct> itemDefs;
    static __int16_t *foreground;
    static __int16_t *background;
    static long posPtr;

    ItemDefinitionStruct GetItemDef(int itemID);

    string name{};
    vector<Player> players{};
    Player local{};
    bool connected{};
    bool requiresTileExtra(int id);
    bool isSpecialTile(int id);
    void setupItemDefs();
    void tileExtraSerialize(uint8_t *extended, int location);
    void tileSerialize(uint8_t *extended, int location);
};
