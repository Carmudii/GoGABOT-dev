#pragma once

#include <string>
#include <vector>
#include "player.h"

using namespace std;

class world {
   public:
    string name{};
    vector<player> players{};
    player local{};
    bool connected{};
};