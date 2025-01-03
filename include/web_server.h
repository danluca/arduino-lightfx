//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_WEB_SERVER_H
#define LIGHTFX_WEB_SERVER_H

#include <WiFiNINA.h>
#include <ArduinoJson.h>

namespace web {

    typedef size_t (*reqHandler)(WiFiClient*, const String*, String*, String*);

    size_t handleGetConfig(WiFiClient *client, const String *uri, String *hd, String *bdy);
    size_t handleGetStatus(WiFiClient *client, const String *uri, String *hd, String *bdy);
    size_t handleGetCss(WiFiClient *client, const String *uri, String *hd, String *bdy);
    size_t handleGetJs(WiFiClient *client, const String *uri, String *hd, String *bdy);
    size_t handleGetHtml(WiFiClient *client, const String *uri, String *hd, String *bdy);
    size_t handleGetRoot(WiFiClient *client, const String *uri, String *hd, String *bdy);
    size_t handlePutConfig(WiFiClient *client, const String *uri, String *hd, String *bdy);

    size_t handleInternalError(WiFiClient *client, const String *uri, const char * message);
    size_t handleNotFoundError(WiFiClient *client, const String *uri, const char *message);
    size_t transmitJsonDocument(JsonVariantConst source, WiFiClient *client);

    void dispatch();
}

#endif //LIGHTFX_WEB_SERVER_H
