#pragma once

#include <string>
#include "proton/vector.hpp"

using namespace std;

class Player {
   public:
    string name{};
    string country{};
    int netid = -1;
    int userid = -1;
    vector2_t pos{};
    vector2_t lastPos{};
    bool invis{};
    bool mod{};
    Player() {
    
    }
    bool operator==(const Player& right) {
        return netid == right.netid && userid == right.userid;
    }
    Player(std::string name, int netid, int uid) {
        this->name = name;
        this->netid = netid;
        this->userid = uid;
    }
};
