#pragma once

#include <string>
#include <ncurses.h>
#include <mutex>
#include <condition_variable>
#include "proton/variant.hpp"
#include "enet/include/enet.h"
#include "world.h"
#include "packet.h"

using namespace std;

enum ConnectionStatusEnum
{
    SERVER_CONNECTING,
    SERVER_CONNECTED,
    SERVER_DISCONNECT,
    SERVER_FAILED,
};

class server
{
private:
    ENetHost *m_server_host;
    ENetPeer *m_server_peer;
    int server_status;
    
    void disconnect(bool reset);
    bool start_client();
    
public:
    struct Item
    {
        uint16_t id;
        uint8_t count;
        uint8_t type;
    };
    static std::vector<server::Item> inventory;
    static std::vector<string> playerName;
    static std::mutex mtx;
    static std::condition_variable cv;
    int m_user = 0;
    int m_token = 0;
    int dropedItemCounter = -1;
    string m_doorID = "";
    string m_server = "213.179.209.168";
    int m_port = 17198;
    world m_world;
    void quit();
    bool connect();
    void redirect_server(variantlist_t &varlist);
    void reconnecting();
    void send(int32_t type, gameupdatepacket_t *data, int32_t len);
    void send(variantlist_t &list, int32_t netid = -1, int32_t delay = 0);
    void send(string packet, int32_t type = 2);
    void setTargetWorld(string worldName);
    void eventHandle();
    string getServerStatus();
};

extern server *g_server;
