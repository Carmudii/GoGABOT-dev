#include <algorithm>
#include <chrono>
#include <random>
#include <regex>
#include <fstream>
#include <ncurses.h>
#include "utils.h"
#include "proton/variant.hpp"
#include "server.h"
#include "json.hpp"
#include "gt.hpp"

using namespace std;
using json = nlohmann::json;

const char hexmap_s[17] = "0123456789abcdef";
mt19937 rng;

gameupdatepacket_t *utils::getStruct(ENetPacket *packet)
{
    if (packet->dataLength < sizeof(gameupdatepacket_t) - 4)
        return nullptr;
    gametankpacket_t *tank = reinterpret_cast<gametankpacket_t *>(packet->data);
    gameupdatepacket_t *gamepacket = reinterpret_cast<gameupdatepacket_t *>(packet->data + 4);
    if (gamepacket->m_packet_flags & 8)
    {
        if (packet->dataLength < gamepacket->m_data_size + 60)
        {
            return nullptr;
        }
        return reinterpret_cast<gameupdatepacket_t *>(&tank->m_data);
    }
    else
        gamepacket->m_data_size = 0;
    return gamepacket;
}

bool utils::replace(string &str, const string &from, const string &to)
{
    size_t start_pos = str.find(from);
    if (start_pos == string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

bool utils::isNumber(const string &s)
{
    return !s.empty() && find_if(s.begin() + (*s.data() == '-' ? 1 : 0), s.end(), [](char c) { return !isdigit(c); }) == s.end();
}

void utils::saveUserInfo(string username, string password, string ownerName)
{
    ofstream file;
    file.open(gt::configuration_file_name);
    json userInfo;
    userInfo["username"] = username;
    userInfo["password"] = password;
    userInfo["owner_name"] = ownerName;
    file << userInfo;
    file.close();
}

/* **************** INT ******************* */

int utils::getNetIDFromVector(string *playerName)
{
    for (auto &player : g_server->m_world.players)
    {
        if (utils::toUpper(player.name) == utils::toUpper(*playerName))
            return player.netid;
    }
    return 0;
}

int utils::random(int min, int max)
{
    if (DO_ONCE)
        rng.seed((unsigned int)chrono::high_resolution_clock::now().time_since_epoch().count());
    uniform_int_distribution<int> distribution(min, max);
    return distribution(rng);
}

uint32_t utils::hash(uint8_t *str, uint32_t len)
{
    if (!str)
        return 0;
    uint8_t *n = (uint8_t *)str;
    uint32_t acc = 0x55555555;
    if (!len)
        while (*n)
            acc = (acc >> 27) + (acc << 5) + *n++;
    else
        for (uint32_t i = 0; i < len; i++)
            acc = (acc >> 27) + (acc << 5) + *n++;
    return acc;
}

/* **************** STRING ******************* */

char *utils::getText(ENetPacket *packet)
{
    gametankpacket_t *tank = reinterpret_cast<gametankpacket_t *>(packet->data);
    memset(packet->data + packet->dataLength - 1, 0, 1);
    return static_cast<char *>(&tank->m_data);
}

string utils::toUpper(string str)
{
    string text = str;
    transform(text.begin(), text.end(), text.begin(), ::toupper);
    return text;
}

string utils::stripMessage(string msg)
{
    regex e("\\x60[a-zA-Z0-9!@#$%^&*()_+\\-=\\[\\]\\{};':\"\\\\|,.<>\\/?]");
    string result = regex_replace(msg, e, "");
    result.erase(std::remove(result.begin(), result.end(), '`'), result.end());
    return result;
}

string utils::getValueFromPattern(string from, string pattern)
{
    regex regexp(pattern);
    smatch m;
    regex_search(from, m, regexp);
    return m[1];
}

string utils::generateQuotes(string text)
{
    return text.insert(text.length(), rand() % 10, ' ') + "'";
}

string utils::colorStr(string str)
{
    string chrs = "0123456789bwpo^$#@!qertas";
    char* x;
    x = (char*)malloc(2);
    x[0] = chrs[rand() % chrs.length()];
    x[1] = 0;
    string y = x;
    free(x);
    return "`" + y + str;
}

string utils::generateRid()
{
    string rid_str;
    
    for (int i = 0; i < 16; i++)
        rid_str += utils::hexStr(utils::random(0, 255));
    
    transform(rid_str.begin(), rid_str.end(), rid_str.begin(), ::toupper);
    
    return rid_str;
}

string utils::generateMac(const string &prefix)
{
    string x = prefix + ":";
    for (int i = 0; i < 5; i++)
    {
        x += utils::hexStr(utils::random(0, 255));
        if (i != 4)
            x += ":";
    }
    return x;
}

string utils::hexStr(unsigned char data)
{
    string s(2, ' ');
    s[0] = hexmap_s[(data & 0xF0) >> 4];
    s[1] = hexmap_s[data & 0x0F];
    return s;
}

string utils::random(uint32_t length)
{
    static auto randchar = []() -> char {
        const char charset[] =
        "0123456789"
        "qwertyuiopasdfghjklzxcvbnm"
        "QWERTYUIOPASDFGHJKLZXCVBNM";
        const uint32_t max_index = (sizeof(charset) - 1);
        return charset[utils::random(INT16_MAX, INT32_MAX) % max_index];
    };
    
    string str(length, 0);
    generate_n(str.begin(), length, randchar);
    return str;
}

/* **************** VECTOR ******************* */

vector<string> utils::split(string str, string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    string token;
    vector<string> res;
    
    while ((pos_end = str.find(delimiter, pos_start)) != string::npos) {
        token = str.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }
    
    res.push_back (str.substr(pos_start));
    return res;
}
