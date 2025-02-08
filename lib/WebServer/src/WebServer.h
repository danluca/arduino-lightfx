/*
    WebServer.h - Create a WebServer class
    Copyright (c) 2022 Earle F. Philhower, III All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
    Modified 8 May 2015 by Hristo Gochkov (proper post and file upload handling)
*/

#pragma once

#include "HTTPServer.h"
#include "detail/mimetable.h"

#define DEFAULT_HTTP_PORT 80

class WebServer : public HTTPServer {
public:
    using ClientType = WiFiClient;
    using ServerType = WiFiServer;

    explicit WebServer(int port = DEFAULT_HTTP_PORT);
    ~WebServer() override;

    virtual void begin();
    virtual void begin(uint16_t port);
    virtual void handleClient();

    virtual void close();
    virtual void stop();

    ServerType &getServer() {
        return _server;
    }

    ClientType& client() override {
        // _currentClient is always a WiFiClient, so we need to coerce to the proper type
        return static_cast<ClientType &>(_currentClient);
    }

private:
    ServerType _server;
};

