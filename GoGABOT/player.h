#pragma once

#include <string>
#include "proton/vector.hpp"

using namespace std;

class Player {
   public:
    string name{};
    string country{};
    vector2_t pos{};
    vector2_t lastPos{};
    int netid = -1;
    int userid = -1;
    int packetFlag = -1;
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
