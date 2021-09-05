#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <fstream>
#include "enet/include/enet.h"
#include "packet.h"
#include "proton/variant.hpp"

#define PRINTS(msg, ...) printf("[SERVER]: " msg, ##__VA_ARGS__);
#define PRINTD(msg, ...) printf("[DEBUG]: " msg, ##__VA_ARGS__);
#define MALLOC(type, ...) (type *)(malloc(sizeof(type) __VA_ARGS__))
#define get_packet_type(packet) (packet->dataLength > 3 ? *packet->data : 0)
#define DO_ONCE            \
([]() {                \
static char o = 0; \
return !o && ++o;  \
}())
#ifdef _WIN32
#define INLINE __forceinline
#else //for gcc/clang
#define INLINE inline
#endif

using namespace std;

namespace utils
    {
    INLINE uint8_t *get_extended(gameupdatepacket_t *packet)
    {
        return reinterpret_cast<uint8_t *>(&packet->m_data_size);
    }
    
    INLINE bool ifExists(const string &directory)
    {
        ifstream isExists(directory.c_str());
        return isExists.good();
    }
    
    vector<string> split(string str, string delimiter);
    
    char *getText(ENetPacket *packet);
    int random(int min, int max);
    int getNetIDFromVector(string *playerName);
    
    uint32_t hash(uint8_t *str, uint32_t len);
    string generateRid();
    string generateMac(const std::string &prefix = "02");
    string hexStr(unsigned char data);
    string random(uint32_t length);
    string toUpper(string str);
    string stripMessage(string msg);
    string getValueFromPattern(string from, string pattern);
    string generateQuotes(string text);
    string colorStr(string str);
    string split(string message, string command, int index);
    
    bool replace(std::string &str, const std::string &from, const std::string &to);
    bool isNumber(const std::string &s);
    void saveUserInfo(string username, string password, string ownerName, int blockID);
    
    gameupdatepacket_t *getStruct(ENetPacket *packet);
    } // namespace utils
