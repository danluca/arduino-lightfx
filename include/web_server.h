//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_WEB_SERVER_H
#define LIGHTFX_WEB_SERVER_H

#include <WiFi.h>
#include <ArduinoJson.h>
#include <regex>

namespace web {

    typedef size_t (*reqHandler)(WiFiClient*, String*, String*, String*);

    size_t handleGetConfig(WiFiClient *client, String *uri, String *hd, String *bdy);
    size_t handleGetStatus(WiFiClient *client, String *uri, String *hd, String *bdy);
    size_t handleGetCss(WiFiClient *client, String *uri, String *hd, String *bdy);
    size_t handleGetJs(WiFiClient *client, String *uri, String *hd, String *bdy);
    size_t handleGetHtml(WiFiClient *client, String *uri, String *hd, String *bdy);
    size_t handleGetRoot(WiFiClient *client, String *uri, String *hd, String *bdy);
    size_t handlePutConfig(WiFiClient *client, String *uri, String *hd, String *bdy);

    size_t handleInternalError(WiFiClient *client, String *uri, const char * message);
    size_t handleNotFoundError(WiFiClient *client, String *uri, const char *message);
    size_t transmitJsonDocument(JsonVariantConst source, WiFiClient *client);

    void dispatch();
}

#endif //LIGHTFX_WEB_SERVER_H
