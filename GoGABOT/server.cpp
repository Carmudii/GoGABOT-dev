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
std::vector<World::DroppedItemStruct> server::DroppedItem;
std::vector<string> server::playerName;
std::mutex server::mtx;
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
                server_status = SERVER_CONNECTED;
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
                        } break;
                    case NET_MESSAGE_GAME_MESSAGE:
                        if (events::send::onGameMessage(utils::getText(event.packet)))
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
                                if (events::send::onState(packet))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                } break;
                            case PACKET_CALL_FUNCTION:
                                if (events::send::variantList(packet))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                } break;
                            case PACKET_PING_REQUEST:
                                if (events::send::onPingReply(packet))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                } break;
                            case PACKET_SEND_INVENTORY_STATE:
                            {
                                server::inventory.clear();
                                auto extended_ptr = utils::get_extended(packet);
                                inventory.resize(*reinterpret_cast<short *>(extended_ptr + 9));
                                memcpy(inventory.data(), extended_ptr + 11, server::inventory.capacity() * sizeof(Item));
                                for (Item& item : inventory) {
                                    if (item.id == gt::block_id) {
                                        totalBlocksInInventory = item.count;
                                    } else if (item.id == (gt::block_id + 1)) {
                                        droppedItemCounter = item.count;
                                    }
                                }
                            } break;
                            case PACKET_SEND_MAP_DATA:
                                if (events::send::onSendMapData(packet, event.packet->dataLength))
                                {
                                    enet_packet_destroy(event.packet);
                                    return;
                                } break;
                            case PACKET_ITEM_CHANGE_OBJECT: {
                                if (packet->m_player_flags == -1) {
                                    droppedItemUID++;
                                    if(gt::is_auto_collect &&
                                       packet->m_vec_x != 0 &&
                                       packet->m_vec_y != 0 &&
                                       packet->m_vec_y <= (m_world.local.lastPos.m_y - 32) &&
                                       packet->m_vec_x >= (m_world.local.lastPos.m_x - 80) &&
                                       packet->m_vec_x <= (m_world.local.lastPos.m_x + 80))
                                    {
                                        if (packet->m_int_data == gt::block_id) totalBlocksInInventory += (int)packet->m_struct_flags; // block
                                        if (packet->m_int_data == gt::block_id + 1) droppedItemCounter += (int)packet->m_struct_flags; // seed
                                        events::send::onSendCollectDropItem(packet->m_vec_x, packet->m_vec_y);
                                    }
                                    if (gt::is_auto_drop && droppedItemCounter >= gt::max_dropped_block) {
                                        send("action|drop\n|itemID|" + to_string(gt::block_id+1));
                                        droppedItemCounter = 0;
                                    }
                                    enet_packet_destroy(event.packet);
                                    return;
                                }
                            } break;
                            case PACKET_TILE_CHANGE_REQUEST: {
                                if (packet->m_player_flags == m_world.local.netid && packet->m_int_data == gt::block_id) {
                                    totalBlocksInInventory--;
                                }
                            } break;
                            case PACKET_APP_INTEGRITY_FAIL:
                                if (gt::in_game) return;
                                break;
                            default:
                                break;
                        }
                    } break;
                    case NET_MESSAGE_TRACK:
                    {
                        rtvar var = rtvar::parse(utils::getText(event.packet));
                        string eventName = var.find("eventName")->m_value;
                        if (eventName == "305_DONATIONBOX") {
                            totalBlocksInInventory += var.get_int("itemamount");
                            gt::block_id = var.get_int("item");
                        } else {
                            // We don't need to handle all tracking packet
                            events::send::onPlayerEnterGame();
                        }
                        enet_packet_destroy(event.packet);
                        return;
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
                reconnecting(false);
                server_status = SERVER_DISCONNECT;
            } break;
            default:
                break;
        }
    }
}

void server::quit()
{
    gt::in_game = false;
    gt::is_exit = true;
    std::__fs::filesystem::remove(gt::configuration_file_name);
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
    if (varlist[2].get_uint32() != -1)
    {
        m_token = varlist[2].get_uint32();
    }
    
    auto doorid = str.substr(str.find("|"));
    m_server = str.erase(str.find("|")); //remove | and doorid from end
    
    gt::connecting = true;
    reconnecting(false);
}

void server::reconnecting(bool reset)
{
    gt::connecting = false;
    m_world.connected = false;
    m_world.players.clear();
    DroppedItem.clear();
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
    
    if (!connect()) {
        reconnecting(false);
    }
}

bool server::connect()
{
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
            return "OK!";
        case SERVER_DISCONNECT:
            return "DC!";
        case SERVER_FAILED:
            return "FAILED!";
    }
    return "...";
}

void server::send(int32_t type, uint8_t* data, int32_t len) {
    std::lock_guard<std::mutex> lock(mtx);

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
    std::lock_guard<std::mutex> lock(mtx);
    
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

void server::send(std::string text, int32_t type) {
    std::lock_guard<std::mutex> lock(mtx);

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
    g_server->send("action|join_request\nname|" + worldName, 3);
}

bool server::inRange(float x, float y, int distanceX, int distanceY) {
    float minX = m_world.local.pos.m_x - distanceX;
    float maxX = m_world.local.pos.m_x + distanceX;
    float minY = m_world.local.pos.m_y - distanceY;
    float maxY = m_world.local.pos.m_y + distanceY;
    return x >= minX && x <= maxX && y >= minY && y <= maxY;
}
