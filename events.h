#pragma once
#include <string>
#include "packet.h"

using namespace std;

namespace events {

    namespace out {
        bool variantList(gameupdatepacket_t* packet);
        bool pingReply(gameupdatepacket_t* packet);
        bool genericText(string packet);
        bool gameMessage(string packet);
        bool state(gameupdatepacket_t* packet);
        void onLoginRequest(string username, string password);
        void onPlayerEnterGame();
    };
};