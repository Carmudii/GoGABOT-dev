#pragma Once

#include <string>
#include "packet.h"

using namespace std;

namespace events
    {
    namespace send
        {
        bool VariantList(gameupdatepacket_t *packet);
        bool OnPingReply(gameupdatepacket_t *packet);
        bool OnGenericText(string packet);
        bool OnGameMessage(string packet);
        bool OnState(gameupdatepacket_t *packet);
        void OnLoginRequest();
        void OnPlayerEnterGame();
        bool OnSendMapData(gameupdatepacket_t *packet, long packetLength);
        void OnSendChatPacket();
        void OnSendPlacePacket();
        void OnSendMessagePacket();
        void OnSendTileChangeRequestPacket();
        void OnSendPacketMove(float posX, float posY, int characterState);
        void OnSendTileActiveRequest(int posX, int posY);
        bool OnSendCollectDropItem(float posX, float posY);
        bool OnChangeObject(gameupdatepacket_t *packet);
        }; // namespace send
    }; // namespace events
