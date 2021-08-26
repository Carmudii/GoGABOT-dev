#include <iostream>
#include <ncurses.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include "server.h"
#include "events.h"
#include "gt.hpp"
#include "proton/hash.hpp"
#include "proton/rtparam.hpp"
#include "utils.h"

using namespace std;
std::vector<server::Item> server::inventory;
std::vector<string> server::playerName;
std::vector<server::ENetPacketQueueStruct> server::ENetPacketQueue;
std::mutex server::mtx;
std::mutex server::enet_mtx;
std::condition_variable server::cv;


void server::eventHandle()
{
    ENetEvent event;
    while (enet_host_service(m_server_host, &event, 0) > 0)
    {
        m_server_peer = event.peer;
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                this->server_status = SERVER_CONNECTED;
            }
                break;
            case ENET_EVENT_TYPE_RECEIVE:
            {
                int packet_type = get_packet_type(event.packet);
                switch (packet_type)
                {
                    case NET_MESSAGE_SERVER_LOGIN_REQUEST:
                        events::send::onLoginRequest();
                        enet_packet_destroy(event.packet);
                        return;
                    case NET_MESSAGE_GENERIC_TEXT:
                        if (events::send::onGenericText(utils::getText(event.packet)))
                        {
                            enet_packet_destroy(event.packet);
                            return;
                        }
                        break;
                    case NET_MESSAGE_GAME_MESSAGE:
                        if (events::send::onGameMessage(utils::getText(event.packet)))
                        {
                            enet_packet_destroy(event.packet);
                            return;
                        }
                        break;
                    case NET_MESSAGE_GAME_PACKET:
                    {
                        auto packet = utils::getStruct(event.packet);
                        if (!packet)
                            break;
                        switch (packet->m_type)
                        {
                            case PACKET_STATE:
                                if (events::send::onState(packet))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                }
                                break;
                            case PACKET_CALL_FUNCTION:
                                if (events::send::variantList(packet))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                }
                                break;
                            case PACKET_PING_REQUEST:
                                if (events::send::onPingReply(packet))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                }
                                break;
                            case PACKET_SEND_INVENTORY_STATE:
                            {
                                server::inventory.clear();
                                auto extended_ptr = utils::get_extended(packet);
                                inventory.resize(*reinterpret_cast<short *>(extended_ptr + 9));
                                memcpy(inventory.data(), extended_ptr + 11, server::inventory.capacity() * sizeof(Item));
                                for (Item& item : inventory) {
                                    if (item.id == gt::block_id) {
                                        this->dropedItemCounter = item.count;
                                        break;
                                    }
                                }
                            }
                                break;
                            case PACKET_SEND_MAP_DATA:
                                if (events::send::onSendMapData(packet, event.packet->dataLength))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                }
                                break;
                            case PACKET_ITEM_CHANGE_OBJECT: {
                                if (packet->m_player_flags == -1) {
                                    dropedItemUID++;
                                    if(gt::is_auto_collect &&
                                       packet->m_vec_x != 0 &&
                                       packet->m_vec_x != 0 &&
                                       packet->m_int_data != 112 &&
                                       this->inRange(packet->m_vec_x, packet->m_vec_y, 80, 80))
                                    {
                                        if(packet->m_int_data == gt::block_id + 1) this->dropedItemCounter++;
                                        if (gt::is_auto_drop && this->dropedItemCounter >= gt::max_dropped_block) {
                                            this->dropedItemCounter = 0;
                                            this->send("action|drop\n|itemID|" + to_string(gt::block_id+1));
                                        }
                                        events::send::onSendCollectDropItem(packet->m_vec_x, packet->m_vec_y);
                                        enet_packet_destroy(event.packet);
                                        return;
                                    }
                                }
                            } break;
                            case PACKET_APP_INTEGRITY_FAIL:
                                if (gt::in_game)
                                    return;
                                break;
                            default:
                                break;
                        }
                    }
                        break;
                    case NET_MESSAGE_TRACK:
                    {
                        events::send::onPlayerEnterGame();
                        enet_packet_destroy(event.packet);
                        return;
                    }
                        break;
                    default:
                        break;
                }
                
                if (!m_server_host)
                    return;
                enet_peer_send_safe(m_server_peer, 0, event.packet);
            }
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                if (gt::connecting) {
                    g_server->reconnecting();
                    gt::connecting = false;
                } else {
                    this->disconnect(false);
                    gt::is_exit = true;
                }
                this->server_status = SERVER_DISCONNECT;
            }
                break;
            default:
                break;
        }
    }
}

void server::doPacketQueue()
{
    while (true) {
        this_thread::sleep_for(chrono::milliseconds(30));
        this->enet_mtx.lock();
        for (int i = 0; i < this->ENetPacketQueue.size(); i++)
        {
            ENetPacket* packet = ENetPacketQueue[i].e_Packet;
            ENetPeer* peer = ENetPacketQueue[i].peerReceiver;
            if (peer->host && peer) {
                if (peer->state == ENET_PEER_STATE_CONNECTED)
                    enet_peer_send(peer, 0, packet); // Dont flush wait for next service call
            }
        }
        this->ENetPacketQueue.clear();
        this->enet_mtx.unlock();
    }
}

void server::quit()
{
    gt::in_game = false;
    ENetPacketQueue.clear();
    this->disconnect(true);
    gt::is_exit = true;
    std::__fs::filesystem::remove(gt::configuration_file_name);
}

bool server::start_client()
{
    m_server_host = enet_host_create(0, 1, 2, 0, 0);
    m_server_host->usingNewPacket = true;
    if (!m_server_host)
    {
        this->server_status = SERVER_FAILED;
        return false;
    }
    m_server_host->checksum = enet_crc32;
    auto code = enet_host_compress_with_range_coder(m_server_host);
    if (code != 0) {
        this->server_status = SERVER_FAILED;
    }
    
    std::thread packet_thread([this] {doPacketQueue(); });
    if (packet_thread.joinable())
        packet_thread.detach();
    
    enet_host_flush(m_server_host);
    return true;
}

void server::redirect_server(variantlist_t &varlist)
{
    this->disconnect(false);
    m_port = varlist[1].get_uint32();
    m_user = varlist[3].get_uint32();
    auto str = varlist[4].get_string();
    if (varlist[2].get_uint32() != -1)
    {
        m_token = varlist[2].get_uint32();
    }
    
    auto doorid = str.substr(str.find("|"));
    m_server = str.erase(str.find("|")); //remove | and doorid from end
    
    gt::connecting = true;
    if (m_server_host)
    {
        enet_host_destroy(m_server_host);
        m_server_host = nullptr;
    }
    this->connect();
}

void server::reconnecting()
{
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
    ENetAddress address;
    enet_address_set_host(&address, m_server.c_str());
    address.port = m_port;
    if (!this->start_client())
    {
        this->server_status = SERVER_FAILED;
        return false;
    }
    m_server_peer = enet_host_connect(m_server_host, &address, 2, 0);
    if (!m_server_peer)
    {
        this->server_status = SERVER_FAILED;
        return false;
    }
    return true;
}

string server::getServerStatus()
{
    switch (this->server_status)
    {
        case SERVER_CONNECTED:
            return "OK!";
        case SERVER_DISCONNECT:
            return "DC!";
        case SERVER_FAILED:
            return "FAILED!";
    }
    return "...";
}

void server::send(int32_t type, uint8_t* data, int32_t len) {
    auto peer = m_server_peer;
    auto host = m_server_host;
    
    if (!peer || !host)
        return;
    auto packet = enet_packet_create(0, len + 5, ENET_PACKET_FLAG_RELIABLE);
    auto game_packet = (gametextpacket_t*)packet->data;
    game_packet->m_type = type;
    if (data)
        memcpy(&game_packet->m_data, data, len);
    
    memset(&game_packet->m_data + len, 0, 1);
    enet_peer_send_safe(peer, 0, packet);
}

void server::send(variantlist_t& list, int32_t netid, int32_t delay) {
    auto peer = m_server_peer;
    auto host = m_server_host;
    
    if (!peer || !host)
        return;
    
    uint32_t data_size = 0;
    void* data = list.serialize_to_mem(&data_size, nullptr);
    
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
    free(data);
    
    auto packet = enet_packet_create(game_packet, data_size + sizeof(gameupdatepacket_t), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send_safe(peer, 0, packet);
    free(game_packet);
}

void server::send(std::string text, int32_t type) {
    auto peer = m_server_peer;
    auto host = m_server_host;
    
    if (!peer || !host)
        return;
    auto packet = enet_packet_create(0, text.length() + 5, ENET_PACKET_FLAG_RELIABLE);
    auto game_packet = (gametextpacket_t*)packet->data;
    game_packet->m_type = type;
    memcpy(&game_packet->m_data, text.c_str(), text.length());
    
    memset(&game_packet->m_data + text.length(), 0, 1);
    enet_peer_send_safe(peer, 0, packet);
}

void server::enet_peer_send_safe(ENetPeer* peer, enet_uint8 channel, ENetPacket* packet)
{
    this->enet_mtx.lock();
    ENetPacketQueueStruct epqs;
    epqs.e_Packet = packet;
    epqs.peerReceiver = peer;
    ENetPacketQueue.push_back(epqs);
    this->enet_mtx.unlock();
}

void server::setTargetWorld(string worldName)
{
    g_server->send("action|join_request\nname|" + worldName, 3);
}

bool server::inRange(float x, float y, int distanceX, int distanceY) {
    float minX = this->m_world.local.pos.m_x - distanceX;
    float maxX = this->m_world.local.pos.m_x + distanceX;
    float minY = this->m_world.local.pos.m_y - distanceY;
    float maxY = this->m_world.local.pos.m_y + distanceY;
    return x >= minX && x <= maxX && y >= minY && y <= maxY;
}