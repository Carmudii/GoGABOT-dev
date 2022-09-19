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
#include "curl/http_client.hpp"
#include "utils.h"

using namespace std;
vector<PlayerInventory::Item> server::inventory;
vector<World::DroppedItemStruct> server::DroppedItem;
vector<string> server::playerName;
vector<int> server::adminNetID;
mutex server::mtx;
condition_variable server::cv;

void server::eventHandle()
{
    ENetEvent event;
    while (enet_host_service(m_server_host, &event, 5) > 0)
    {
        m_server_peer = event.peer;
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                server_status = SERVER_CONNECTED;
            }
                break;
            case ENET_EVENT_TYPE_RECEIVE:
            {
                int packet_type = get_packet_type(event.packet);
                switch (packet_type)
                {
                    case NET_MESSAGE_SERVER_LOGIN_REQUEST:
                        events::send::OnLoginRequest();
                        enet_packet_destroy(event.packet);
                        return;
                    case NET_MESSAGE_GENERIC_TEXT:
                        if (events::send::OnGenericText(utils::getText(event.packet)))
                        {
                            enet_packet_destroy(event.packet);
                            return;
                        } break;
                    case NET_MESSAGE_GAME_MESSAGE:
                        if (events::send::OnGameMessage(utils::getText(event.packet)))
                        {
                            enet_packet_destroy(event.packet);
                            return;
                        } break;
                    case NET_MESSAGE_GAME_PACKET:
                    {
                        auto packet = utils::getStruct(event.packet);
                        if (!packet)
                            break;
                        switch (packet->m_type)
                        {
                            case PACKET_STATE:
                                if (events::send::OnState(packet))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                } break;
                            case PACKET_CALL_FUNCTION:
                                if (events::send::VariantList(packet))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                } break;
                            case PACKET_PING_REQUEST:
                                if (events::send::OnSendPing(packet))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                } break;
                            case PACKET_SEND_INVENTORY_STATE:
                            {
                                server::inventory.clear();
                                auto extended_ptr = utils::get_extended(packet);
                                inventory.resize(*reinterpret_cast<short *>(extended_ptr + 9));
                                memcpy(inventory.data(), extended_ptr + 11, server::inventory.capacity() * sizeof(PlayerInventory::Item));
                                for (PlayerInventory::Item& item : inventory) {
                                    if (item.id == gt::block_id) {
                                        this->playerInventory.setTotalInventoryBlock(item.count);
                                    } else if (item.id == (gt::block_id + 1)) {
                                        this->playerInventory.setTotalDroppedItem(item.count);
                                    }
                                }
                                MenuBar::refreshStatusWindow(TYPE_BOTTOM);
                            } break;
                            case PACKET_SEND_MAP_DATA:
                                if (events::send::OnSendMapData(packet, event.packet->dataLength))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                } break;
                            case PACKET_ITEM_CHANGE_OBJECT: {
                                if (events::send::OnChangeObject(packet)) {
                                    enet_packet_destroy(event.packet);
                                    return;
                                }
                            } break;
                            case PACKET_TILE_CHANGE_REQUEST: {
                                if (packet->m_player_flags == m_world.local.netid && packet->m_int_data == gt::block_id) {
                                    this->playerInventory.updateInventoryTotalBlock(-1);
                                    MenuBar::refreshStatusWindow(TYPE_BOTTOM);
                                    enet_packet_destroy(event.packet);
                                    return;
                                }
                            } break;
                            case PACKET_APP_INTEGRITY_FAIL:
                                if (gt::in_game) {
                                    enet_packet_destroy(event.packet);
                                    return;
                                }
                                break;
                            default:
                                break;
                        }
                    } break;
                    case NET_MESSAGE_TRACK:
                    {
                        if (events::send::OnSetTrackingPacket(utils::getText(event.packet))) {
                            enet_packet_destroy(event.packet);
                            return;
                        }
                    } break;
                    default:
                        break;
                }
                
                if (!m_server_host)
                    return;
                enet_peer_send(m_server_peer, 0, event.packet);
            } break;
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                this->reconnecting(false);
            } break;
            default:
                enet_packet_destroy(event.packet);
                break;
        }
    }
}

void server::quit()
{
    gt::in_game = false;
    gt::is_exit = true;
//    __fs::filesystem::remove(gt::configuration_file_name);
}

bool server::start_client()
{
    m_server_host = enet_host_create(0, 1, 2, 0, 0);
    m_server_host->usingNewPacket = true;
    
    if (!m_server_host)
    {
        server_status = SERVER_FAILED;
        return false;
    }
    m_server_host->checksum = enet_crc32;
    auto code = enet_host_compress_with_range_coder(m_server_host);
    if (code != 0) {
        server_status = SERVER_FAILED;
    }
        
    enet_host_flush(m_server_host);
    return true;
}

void server::redirect_server(variantlist_t &varlist)
{
    m_port = varlist[1].get_uint32();
    m_user = varlist[3].get_uint32();
    auto str = varlist[4].get_string();
    rtvar var = rtvar::parse(str);
    if (varlist[2].get_uint32() != -1)
    {
        m_token = varlist[2].get_uint32();
    }
    
    PRINTS("%s TOKEN : ", var.find("UUIDToken")->m_value.c_str());

    auto doorid = str.substr(str.find("|"));
    m_server = str.erase(str.find("|")); //remove | and doorid from end
    
    gt::connecting = true;
    this->reconnecting(false);
}

void server::reconnecting(bool reset)
{
    gt::is_admin_entered    = false;
    gt::connecting          = false;
    server_status           = SERVER_DISCONNECT;
    MenuBar::refreshStatusWindow(TYPE_BOTTOM);
    this->m_world.connected = false;
    this->m_world.local = {};
    this->m_world.players.clear();
    this->DroppedItem.clear();
    this->adminNetID.clear();
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
    }
    
    if (!this->connect(reset)) {
        this->reconnecting(false);
    }
}

bool server::connect(bool restartConnection)
{
    if (restartConnection && httpClient->get()) {
        m_server = httpClient->default_host;
        m_port = httpClient->default_port;
        meta = httpClient->meta;
    }
    
    ENetAddress address;
    enet_address_set_host(&address, m_server.c_str());
    address.port = m_port;
    if (!start_client())
    {
        server_status = SERVER_FAILED;
        return false;
    }
    m_server_peer = enet_host_connect(m_server_host, &address, 2, 0);
    if (!m_server_peer)
    {
        server_status = SERVER_FAILED;
        return false;
    }
    
    return true;
}

string server::getServerStatus()
{
    switch (server_status)
    {
        case SERVER_CONNECTED:
            return "ONLINE";
        case SERVER_DISCONNECT:
            return "RECON";
        case SERVER_FAILED:
            return "NO INTERNET";
    }
    return "OFFLINE";
}

void server::send(int32_t type, uint8_t* data, int32_t len) {
    lock_guard<mutex> lock(this->mtx);

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
    enet_peer_send(peer, 0, packet);
}

void server::send(variantlist_t& list, int32_t netid, int32_t delay) {
    lock_guard<mutex> lock(this->mtx);
    
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
    enet_peer_send(peer, 0, packet);
    free(game_packet);
}

void server::send(string text, int32_t type) {
    lock_guard<mutex> lock(this->mtx);

    auto peer = m_server_peer;
    auto host = m_server_host;
    
    if (!peer || !host)
        return;
    auto packet = enet_packet_create(0, text.length() + 5, ENET_PACKET_FLAG_RELIABLE);
    auto game_packet = (gametextpacket_t*)packet->data;
    game_packet->m_type = type;
    memcpy(&game_packet->m_data, text.c_str(), text.length());
    
    memset(&game_packet->m_data + text.length(), 0, 1);
    enet_peer_send(peer, 0, packet);
}

void server::setTargetWorld(string worldName)
{
    this->send("action|join_request\nname|" + worldName, 3);
}

bool server::inRange(float x, float y, int distanceX, int distanceY) {
    float minX = this->m_world.local.pos.m_x - distanceX;
    float maxX = this->m_world.local.pos.m_x + distanceX;
    float minY = this->m_world.local.pos.m_y - distanceY;
    float maxY = this->m_world.local.pos.m_y + distanceY;
    return x >= minX && x <= maxX && y >= minY && y <= maxY;
}
