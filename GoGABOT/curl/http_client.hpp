//
//  http_client.hpp
//  GoGABOT
//
//  Created by Carmudi on 21/11/21.
//  Copyright © 2021 9GATE. All rights reserved.
//

#ifndef http_client_hpp
#define http_client_hpp

#include <stdio.h>
#include <curl/curl.h>
#include "../gt.hpp"

using namespace std;

class HttpClient {
private:
    CURL *curl;
    CURLcode res;
    string postField = "version="+gt::version+"&platform=4&protocol=143";
    string readBuffer;
    long response_code;

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

public:
    string default_host = "213.179.209.168";
    int default_port = 0;
    bool get();
};

#endif /* http_client_hpp */
