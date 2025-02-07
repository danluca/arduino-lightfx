/*
    WebServerTemplate - Makes an actual Server class from a HTTPServer
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

#include <detail/mimetable.h>

#include "../../PicoLog/src/LogProxy.h"
#include "HTTPServer.h"

template<typename ServerType, int DefaultPort = 80> class WebServerTemplate;

template<typename ServerType, int DefaultPort> class WebServerTemplate : public HTTPServer {
public:
    using WebServerType = WebServerTemplate<ServerType>;
    using ClientType = typename ServerType::ClientType;

    explicit WebServerTemplate(IPAddress addr, int port = DefaultPort);
    explicit WebServerTemplate(int port = DefaultPort);
    ~WebServerTemplate() override;

    virtual void begin();
    virtual void begin(uint16_t port);
    virtual void handleClient();

    virtual void close();
    virtual void stop();

    ServerType &getServer() {
        return _server;
    }

    ClientType& client() override {
        // _currentClient is always a WiFiClient, so we need to coerce to the proper type for SSL
        return (ClientType&)_currentClient;
    }

private:
    ServerType _server;
};

template <typename ServerType, int DefaultPort> WebServerTemplate<ServerType, DefaultPort>::WebServerTemplate(IPAddress addr, int port) : HTTPServer(), _server(addr, port) {
}

template <typename ServerType, int DefaultPort> WebServerTemplate<ServerType, DefaultPort>::WebServerTemplate(int port) : HTTPServer(), _server(port) {
}

template <typename ServerType, int DefaultPort> WebServerTemplate<ServerType, DefaultPort>::~WebServerTemplate() {
    _server.close();
}

template <typename ServerType, int DefaultPort> void WebServerTemplate<ServerType, DefaultPort>::begin() {
    close();
    _server.begin();
    _server.setNoDelay(true);
}

template <typename ServerType, int DefaultPort> void WebServerTemplate<ServerType, DefaultPort>::begin(uint16_t port) {
    close();
    _server.begin(port);
    _server.setNoDelay(true);
}

template <typename ServerType, int DefaultPort> void WebServerTemplate<ServerType, DefaultPort>::handleClient() {
    if (_currentStatus == HC_NONE) {
        //we're not managing the current client object - just leveraging the client instance that WiFiNINA library manages
        //should we use available() instead of accept()?
        _currentClient = _server.available();
        if (!_currentClient) {
            if (_nullDelay) {
                delay(1);
            }
            return;
        }
        _currentStatus = HC_WAIT_READ;
        _statusChangeTime = millis();
        log_debug(F("WebServer: new client discovered, current status %d"), _currentStatus);
        //TODO: here we should reset server's internal status
        resetRequestHandling();
    }
    bool keepCurrentClient = false;
    bool shouldYield = false;

    if (_currentClient.connected()) {
        switch (_currentStatus) {
        case HC_NONE:
            // No-op to avoid C++ compiler warning
            break;
        case HC_WAIT_READ:
            // Wait for data from client to become available
            if (_currentClient.available()) {
                _currentClient.setTimeout(HTTP_MAX_SEND_WAIT);
                switch (_parseHandleRequest(&_currentClient)) {
                case CLIENT_REQUEST_CAN_CONTINUE:
                    _contentLength = CONTENT_LENGTH_NOT_SET;
                    _handleRequest();
                /* fallthrough */
                case CLIENT_REQUEST_IS_HANDLED:
                    if (_currentClient.connected() || _currentClient.available()) {
                        _currentStatus = HC_WAIT_CLOSE;
                        _statusChangeTime = millis();
                        keepCurrentClient = true;
                    } else {
                        //Log.debug("webserver: peer has closed after served\n");
                    }
                    break;
                case CLIENT_MUST_STOP:
                    //this is in response to bad inbound request - either from parsing it or handling its raw data
                    send(400, mime::mimeTable[mime::txt].mimeType, _responseCodeToString(400));
                    _currentClient.stop();
                    break;
                case CLIENT_IS_GIVEN:
                    // client must not be stopped but must not be handled here anymore
                    // (example: tcp connection given to websocket)
                    // keepCurrentClient = true;
                    //Log.debug("Give client\n");
                    break;
                } // switch _parseRequest()
            } else {
                // !_currentClient.available(): waiting for more data
                // Use faster connection drop timeout if any other client has data
                // or the buffer of pending clients is full
                if (const unsigned long timeSinceChange = millis() - _statusChangeTime; timeSinceChange > HTTP_MAX_DATA_WAIT) {
                    // log_debug("WebServer: closing after read timeout %d ms", HTTP_MAX_DATA_WAIT);
                } else
                    keepCurrentClient = true;
                shouldYield = true;
            }
            break;
        case HC_WAIT_CLOSE:
            // Wait for client to close the connection
            if (millis() - _statusChangeTime <= HTTP_MAX_CLOSE_WAIT) {
                keepCurrentClient = true;
                shouldYield = true;
            }
        }
    }

    if (!keepCurrentClient) {
        _currentClient.stop();
        _currentStatus = HC_NONE;
        _currentUpload.reset();
        _currentRaw.reset();
    }

    if (shouldYield) {
        yield();
    }

}

template <typename ServerType, int DefaultPort> void WebServerTemplate<ServerType, DefaultPort>::close() {
    _server.close();
    httpClose();
}

template <typename ServerType, int DefaultPort> void WebServerTemplate<ServerType, DefaultPort>::stop() {
    close();
}
