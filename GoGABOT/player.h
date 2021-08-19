#pragma once

#include <string>
#include "proton/vector.hpp"

using namespace std;

class player {
   public:
    string name{};
    string country{};
    int netid = -1;
    int userid = -1;
    vector2_t pos{};
    bool invis{};
    bool mod{};
    player() {
    
    }
    bool operator==(const player& right) {
        return netid == right.netid && userid == right.userid;
    }
    player(std::string name, int netid, int uid) {
        this->name = name;
        this->netid = netid;
        this->userid = uid;
    }
};