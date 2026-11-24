#pragma once
#include "Arduino.h"
struct WebRequest {
    String payload;
    String body() const { return payload; }
};
struct WebClient {
    WebRequest req;
    IPAddress ip{1};
    int code = 0;
    String response;
    const WebRequest &request() const { return req; }
    IPAddress remoteIP() const { return ip; }
    void addResponseHeader(const char *, const char *) {}
    void send(int status, const char *, const String &body) { code = status; response = body; }
};
