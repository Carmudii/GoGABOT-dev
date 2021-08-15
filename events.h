#pragma once

#include <string>
#include "packet.h"

using namespace std;

// class Mutex()
// {
// public:
//     std::mutex m;
//     std::condition_variable cv;

//     Mutex(std::mutex m, std::condition_variable cv)
//     {
//         this->m = m;
//         this->cv = cv;
//     }
// }

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
}; // namespace send
}; // namespace events
