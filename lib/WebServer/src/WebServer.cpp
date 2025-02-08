// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#include "WebServer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "../../PicoLog/src/LogProxy.h"

WebServer::WebServer(const int port) : _server(port) {
}

WebServer::~WebServer() {
    _server.close();
}

void WebServer::begin() {
    close();
    _server.begin();
    _server.setNoDelay(true);
}

void WebServer::begin(const uint16_t port) {
    close();
    _server.begin(port);
    _server.setNoDelay(true);
}

void WebServer::handleClient() {
    if (_currentStatus == HC_NONE) {
        //we're not managing the current client object - just leveraging the client instance produced by WiFiNINA library
        //should we use available() instead of accept()?
        _currentClient = _server.available();
        if (!_currentClient) {
            if (_nullDelay) {
                vTaskDelay(pdMS_TO_TICKS(10));
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

void WebServer::close() {
    _server.close();
    httpClose();
}

void WebServer::stop() {
    close();
}

