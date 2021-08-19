#pragma once

#include <string>
#include "packet.h"

using namespace std;

namespace events
    {
    namespace send
        {
        bool variantList(gameupdatepacket_t *packet);
        bool onPingReply(gameupdatepacket_t *packet);
        bool onGenericText(string packet);
        bool onGameMessage(string packet);
        bool onState(gameupdatepacket_t *packet);
        void onLoginRequest();
        void onPlayerEnterGame();
        bool onSendMapData(gameupdatepacket_t *packet);
        void onSendPuchDamage(gameupdatepacket_t *packet);
        void onSendChatPacket();
        void onSendMessagePacket();
        void onSendPunchPacket();
        void onSendPacketMove(float posX, float posY, int characterState);
        void onSendCollectDropItem(gameupdatepacket_t *packet);
        }; // namespace send
    }; // namespace events
