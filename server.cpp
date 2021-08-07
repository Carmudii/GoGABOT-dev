#include "server.h"
#include <iostream>
#include "events.h"
#include "gt.hpp"
#include "proton/hash.hpp"
#include "proton/rtparam.hpp"
#include "utils.h"

using namespace std;

void server::EventHandle()
{
    ENetEvent event;
    while (enet_host_service(m_server_host, &event, 0) > 0)
    {
        m_server_peer = event.peer;
        PRINTS("ENET EVENT: %d\n", event.type);
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
        {
            PRINTS("Client was connected!\n");
        }
        break;
        case ENET_EVENT_TYPE_RECEIVE:
        {
            int packet_type = get_packet_type(event.packet);
            PRINTS("PACKET TYPE: %d\n", packet_type);
            switch (packet_type)
            {
            case NET_MESSAGE_SERVER_LOGIN_REQUEST:
                events::out::onLoginRequest("PunchBoard1212", "123456Aa!");
                return;
            case NET_MESSAGE_GENERIC_TEXT:
                if (events::out::genericText(utils::get_text(event.packet)))
                {
                    enet_packet_destroy(event.packet);
                    return;
                }
                break;
            case NET_MESSAGE_GAME_MESSAGE:
                if (events::out::gameMessage(utils::get_text(event.packet)))
                {
                    enet_packet_destroy(event.packet);
                    return;
                }
                break;
            case NET_MESSAGE_GAME_PACKET:
            {
                auto packet = utils::get_struct(event.packet);
                if (!packet)
                    break;
                switch (packet->m_type)
                {
                case PACKET_STATE:
                    if (events::out::state(packet))
                    {
                        enet_packet_destroy(event.packet);
                        return;
                    }
                    break;
                case PACKET_CALL_FUNCTION:
                    if (events::out::variantList(packet))
                    {
                        enet_packet_destroy(event.packet);
                        return;
                    }
                    break;
                case PACKET_PING_REQUEST:
                    if (events::out::pingReply(packet))
                    {
                        enet_packet_destroy(event.packet);
                        return;
                    }
                    break;
                case PACKET_DISCONNECT:
                {
                    PRINTS("unhandle case!");
                }
                break;
                case PACKET_APP_INTEGRITY_FAIL:
                    if (gt::in_game)
                        return;
                    break;
                default:
                    PRINTS("gamepacket type: %d\n", packet->m_type);
                }
            }
            break;
            case NET_MESSAGE_TRACK:
            {
                //track one should never be used, but its not bad to have it in case.
                events::out::onPlayerEnterGame();
                return;
            }
            break;
            case NET_MESSAGE_CLIENT_LOG_RESPONSE:
                return;
            default:
                PRINTS("Got unknown packet of type %d.\n", packet_type);
                break;
            }

            if (!m_server_host)
                return;
            enet_peer_send(m_server_peer, 0, event.packet);
            enet_host_flush(m_server_host);
        }
        break;
        case ENET_EVENT_TYPE_DISCONNECT:
        {
            if (gt::in_game)
                return;
            if (gt::connecting)
            {
                this->disconnect(false);
                gt::connecting = false;
                exit(EXIT_FAILURE);
            }
        }
        break;
        default:
            PRINTS("UNHANDLED %d\n", event.type);
            break;
        }
    }
}

void server::quit()
{
    gt::in_game = false;
    this->disconnect(true);
}

bool server::start_client()
{
    m_server_host = enet_host_create(0, 1, 2, 0, 0);
    m_server_host->usingNewPacket = true;
    if (!m_server_host)
    {
        PRINTC("failed to start the client\n");
        return false;
    }
    m_server_host->checksum = enet_crc32;
    auto code = enet_host_compress_with_range_coder(m_server_host);
    if (code != 0)
        PRINTC("enet host compressing failed\n");
    enet_host_flush(m_server_host);
    PRINTC("Started enet client\n");
    return true;
}

void server::redirect_server(variantlist_t &varlist)
{
    this->disconnect(false);
    m_port = varlist[1].get_uint32();
    m_token = varlist[2].get_uint32();
    m_user = varlist[3].get_uint32();
    auto str = varlist[4].get_string();

    auto doorid = str.substr(str.find("|"));
    m_server = str.erase(str.find("|")); //remove | and doorid from end
    PRINTC("port: %d token %d user %d server %s doorid %s\n", m_port, m_token, m_user, m_server.c_str(), doorid.c_str());

    gt::connecting = true;
    if (m_server_host)
    {
        enet_host_destroy(m_server_host);
        m_server_host = nullptr;
    }
    this->connect();
}

void server::disconnect(bool reset)
{
    m_world.connected = false;
    m_world.local = {};
    m_world.players.clear();
    if (m_server_peer)
    {
        enet_peer_disconnect(m_server_peer, 0);
        m_server_peer = nullptr;
        enet_host_destroy(m_server_host);
        m_server_host = nullptr;
    }
    if (reset)
    {
        m_user = 0;
        m_token = 0;
        m_server = "213.179.209.168";
        m_port = 17198;
    }
}

bool server::connect()
{
    PRINTS("Connecting to server.\n");
    ENetAddress address;
    enet_address_set_host(&address, m_server.c_str());
    address.port = m_port;
    PRINTS("port is %d and server is %s\n", m_port, m_server.c_str());
    if (!this->start_client())
    {
        PRINTS("Failed to setup client when trying to connect to server!\n");
        return false;
    }
    m_server_peer = enet_host_connect(m_server_host, &address, 2, 0);
    if (!m_server_peer)
    {
        PRINTS("Failed to connect to real server.\n");
        return false;
    }
    return true;
}

void server::send(int32_t type, uint8_t *data, int32_t len)
{
    auto peer = m_server_peer;
    auto host = m_server_host;

    if (!peer || !host)
        return;
    auto packet = enet_packet_create(0, len + 5, ENET_PACKET_FLAG_RELIABLE);
    auto game_packet = (gametextpacket_t *)packet->data;
    game_packet->m_type = type;
    if (data)
        memcpy(&game_packet->m_data, data, len);

    memset(&game_packet->m_data + len, 0, 1);
    int code = enet_peer_send(peer, 0, packet);
    if (code != 0)
        PRINTS("Error sending packet! code: %d\n", code);
    enet_host_flush(host);
}

void server::send(variantlist_t &list, int32_t netid, int32_t delay)
{
    auto peer = m_server_peer;
    auto host = m_server_host;

    if (!peer || !host)
        return;

    uint32_t data_size = 0;
    void *data = list.serialize_to_mem(&data_size, nullptr);

    //optionally we wouldnt allocate this much but i dont want to bother looking into it
    auto update_packet = MALLOC(gameupdatepacket_t, +data_size);
    auto game_packet = MALLOC(gametextpacket_t, +sizeof(gameupdatepacket_t) + data_size);

    if (!game_packet || !update_packet)
        return;

    memset(update_packet, 0, sizeof(gameupdatepacket_t) + data_size);
    memset(game_packet, 0, sizeof(gametextpacket_t) + sizeof(gameupdatepacket_t) + data_size);
    game_packet->m_type = NET_MESSAGE_GAME_PACKET;

    update_packet->m_type = PACKET_CALL_FUNCTION;
    update_packet->m_player_flags = netid;
    update_packet->m_packet_flags |= 8;
    update_packet->m_int_data = delay;
    memcpy(&update_packet->m_data, data, data_size);
    update_packet->m_data_size = data_size;
    memcpy(&game_packet->m_data, update_packet, sizeof(gameupdatepacket_t) + data_size);
    free(update_packet);

    auto packet = enet_packet_create(game_packet, data_size + sizeof(gameupdatepacket_t), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(host);
    free(game_packet);
}

void server::send(std::string text, int32_t type)
{
    auto peer = m_server_peer;
    auto host = m_server_host;

    if (!peer || !host)
        return;
    auto packet = enet_packet_create(0, text.length() + 5, ENET_PACKET_FLAG_RELIABLE);
    auto game_packet = (gametextpacket_t *)packet->data;
    game_packet->m_type = type;
    memcpy(&game_packet->m_data, text.c_str(), text.length());

    memset(&game_packet->m_data + text.length(), 0, 1);
    int code = enet_peer_send(peer, 0, packet);
    if (code != 0)
        PRINTS("Error sending packet! code: %d\n", code);
    enet_host_flush(host);
}
