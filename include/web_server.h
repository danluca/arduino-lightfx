//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_WEB_SERVER_H
#define LIGHTFX_WEB_SERVER_H

#include <ArduinoJson.h>
#include <RestWebServer.h>

namespace web {
    extern WebServer server;
    extern bool server_handlers_configured;

    void server_setup();
    void webserver();
    void handleGetStatus(WebClient& client);
    void handlePutConfig(WebClient& client);
    void handleGetTasks(WebClient& client);
    void handleNotFound(WebClient& client);
    size_t marshalJson(const JsonDocument &doc, WebClient &client);
}

#endif //LIGHTFX_WEB_SERVER_H
