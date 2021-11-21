//
//  http_client.cpp
//  GoGABOT
//
//  Created by Carmudi on 21/11/21.
//  Copyright © 2021 9GATE. All rights reserved.
//

#include <iostream>
#include <string>
#include "../proton/hash.hpp"
#include "../proton/rtparam.hpp"
#include "../proton/variant.hpp"
#include "../gt.hpp"
#include "http_client.hpp"

size_t HttpClient::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool HttpClient::get()
{
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://growtopia1.com/growtopia/server_data.php");
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postField.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code == 200) {
                rtvar var = rtvar::parse(readBuffer);
                default_host = var.find("server")->m_value;
                default_port = var.get_int("port");
                curl_easy_cleanup(curl);
                return true;
            }
        }
        curl_easy_cleanup(curl);
    }
    return false;
}
