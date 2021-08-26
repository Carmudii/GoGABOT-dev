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
        bool onSendMapData(gameupdatepacket_t *packet, long packetLength);
        void onSendChatPacket();
        void onSendPlacePacket();
        void onSendMessagePacket();
        void onSendTileChangeRequestPacket();
        void onSendPacketMove(float posX, float posY, int characterState);
        bool onSendCollectDropItem(float posX, float posY);
        }; // namespace send
    }; // namespace events
