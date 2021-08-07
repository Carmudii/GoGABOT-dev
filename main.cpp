#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include "enet/include/enet.h"
#include "server.h"

server* g_server = new server();

int main() {
#ifdef _WIN32
    SetConsoleTitleA("GoGABOT");
#endif

    enet_initialize();
    if (g_server->connect()) {
        printf("Server & client proxy is running.\n");
        while (true) {
            g_server->EventHandle();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }   
    else
        printf("Failed to start server or proxy.\n");
}
