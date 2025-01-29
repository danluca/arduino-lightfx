//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_WEB_SERVER_H
#define LIGHTFX_WEB_SERVER_H

#include <ArduinoJson.h>
#include <WebServer.h>

namespace web {
    extern WebServer server;
    extern bool server_handlers_configured;

    void server_setup();
    void webserver();
    void handleGetStatus();
    void handlePutConfig();
    void handleGetTasks();
    void handleNotFound();
    size_t marshalJson(const JsonDocument &doc, WebServer &srv);
}

#endif //LIGHTFX_WEB_SERVER_H
