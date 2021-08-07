#pragma once
#include <string>
#include "proton/variant.hpp"
#include "enet/include/enet.h"
#include "world.h"

using namespace std;

class server
{
private:
    ENetHost *m_server_host;
    ENetPeer *m_server_peer;

    void disconnect(bool reset);
    bool start_client();

public:
    struct Item
    {
        uint16_t id;
        uint8_t count;
        uint8_t type;
    };
    int m_user = 0;
    int m_token = 0;
    string m_doorID = ""; 
    string m_server = "213.179.209.168";
    int m_port = 17198;
    world m_world;
    void quit();
    bool connect();
    void redirect_server(variantlist_t &varlist);
    void send(int32_t type, uint8_t *data, int32_t len);
    void send(variantlist_t &list, int32_t netid = -1, int32_t delay = 0);
    void send(string packet, int32_t type = 2);
    void EventHandle();
};
extern server *g_server;
