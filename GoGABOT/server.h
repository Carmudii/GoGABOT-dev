#pragma once

#include <string>
#include <ncurses.h>
#include <mutex>
#include <condition_variable>
#include "proton/variant.hpp"
#include "enet/include/enet.h"
#include "world.h"
#include "packet.h"
#include "Ui/menu.h"
#include "playerInventory.h"
#include "curl/http_client.hpp"

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
    ENetSocks5Config config;
    ENetHost *m_server_host;
    ENetPeer *m_server_peer;

    int server_status;
    bool start_client();
    HttpClient *httpClient = new HttpClient();

public:
    static vector<World::DroppedItemStruct> DroppedItem;
    static vector<PlayerInventory::Item> inventory;
    static vector<string> playerName;
    static vector<int> adminNetID;
    static mutex mtx;
    static condition_variable cv;
    
    string meta = "ubisoft.com";
    string m_server = "213.179.209.168";
    string mac = "a4:5e:60:b7:43:99";
    string m_doorID = "";
    string m_uuid_token = "";
    string getServerStatus();

    int m_port = 0;
    int level = 0;
    int gems = 0;
    int m_user = 0;
    int m_token = 0;
    int donationBoxPosition = 0;
    
    bool inRange(float x, float y, int distanceX = 100, int distanceY = 100);
    bool connect(bool restartConnection = false);
    void quit();
    void redirect_server(variantlist_t &varlist);
    void send(int32_t type, uint8_t* data, int32_t len);
    void send(variantlist_t& list, int32_t netid = -1, int32_t delay = 0);
    void send(string packet, int32_t type = 2);
    void eventHandle();
    void setTargetWorld(string worldName);
    void reconnecting(bool reset);
        
    World m_world;
    PlayerInventory playerInventory;
    vector2_t lastPos{};
};

extern server *g_server;
