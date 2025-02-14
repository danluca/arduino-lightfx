/*
    HTTPServer.cpp - Dead simple web-server.
    Supports only one simultaneous client, knows how to handle GET and POST.

    Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
    Copyright (c) 2025 Dan Luca. All rights reserved.

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

#include <Arduino.h>
#include "WiFiClient.h"
#include "HTTPServer.h"
#include "FS.h"
#include "detail/RequestHandlers.h"
#include "LogProxy.h"

static constexpr auto Canned503Response PROGMEM = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n";

HTTPServer::HTTPServer() : _corsEnabled(false), _nullDelay(true), _crossOrigin(true), _port(DEFAULT_HTTP_PORT) {
    log_debug("HTTPServer::HTTPServer()");
}

HTTPServer::~HTTPServer() {
    _headersOfInterest.clear();
    //delete each element of _requestHandlers - it has been created by this class
    for (const auto& handler : _requestHandlers) {
        delete handler;
    }
    _requestHandlers.clear();
    //delete any clients left in the queue
    for (const auto& client : _clients) {
        delete client;
    }
    _clients.clear();
    _server.close();
}
void HTTPServer::begin() {
    _server.close();
    _server.begin(_port);
    _server.setNoDelay(true);
    _state = IDLE;
}

void HTTPServer::begin(const uint16_t port) {
    _server.close();
    _port = port;
    _server.begin(_port);
    _server.setNoDelay(true);
    _state = IDLE;
}

void HTTPServer::close() {
    httpClose();
    _server.close();
}

void HTTPServer::stop() {
    close();
}

/**
 * Establishes a handler for the given URI
 * @param uri the request URI to service
 * @param fn handler of the given request URI
 * @return a request handler
 */
RequestHandler& HTTPServer::on(const Uri &uri, const THandlerFunction &fn) {
    return on(uri, HTTP_ANY, fn);
}

/**
 * Establishes a handler for the given URI and HTTP method
 * @param uri
 * @param method
 * @param fn
 * @return
 */
RequestHandler& HTTPServer::on(const Uri &uri, const HTTPMethod method, const THandlerFunction &fn) {
    return on(uri, method, fn, _fileUploadHandler);
}

/**
 * Establishes a handler for the given URI and HTTP method that allows for uploading files
 * @param uri
 * @param method
 * @param fn
 * @param ufn
 * @return
 */
RequestHandler& HTTPServer::on(const Uri &uri, const HTTPMethod method, const THandlerFunction &fn, const THandlerFunction &ufn) {
    auto *handler = new FunctionRequestHandler(fn, ufn, uri, method);
    _addRequestHandler(handler);
    return *handler;
}

bool HTTPServer::removeRoute(const String &uri, const HTTPMethod method) {
    bool anyHandlerRemoved = false;
    for (auto it = _requestHandlers.begin(); it != _requestHandlers.end(); ++it) {
        if (const RequestHandler *handler = *it; handler->match(uri, method)) {
            delete handler;
            _requestHandlers.erase(it);
            anyHandlerRemoved = true;
        }
    }
    return anyHandlerRemoved;
}

void HTTPServer::addHandler(RequestHandler* handler) {
    _addRequestHandler(handler);
}

bool HTTPServer::removeHandler(const RequestHandler *handler) {
    return _removeRequestHandler(handler);
}

void HTTPServer::_addRequestHandler(RequestHandler* handler) {
    _requestHandlers.push_back(handler);
}

bool HTTPServer::_removeRequestHandler(const RequestHandler *handler) {
    for (auto it = _requestHandlers.begin(); it != _requestHandlers.end(); ++it) {
        if (*it == handler) {
            delete handler;
            _requestHandlers.erase(it);
            return true;
        }
    }
    return false;
}

/**
 * Establishes a handler for the given URI of static resources - HTML, images, CSS, JS, etc.
 * @param uri
 * @param fs
 * @param path
 * @param memRes
 * @param cache_header
 */
void HTTPServer::serveStatic(const char* uri, const SynchronizedFS& fs, const char* path, const std::map<std::string, const char*> *memRes, const char* cache_header) {
    _addRequestHandler(new StaticSyncFileRequestHandler(fs, path, uri, cache_header));
    if (memRes)
        _addRequestHandler(new StaticInMemoryRequestHandler(*memRes, uri, cache_header));
}

void HTTPServer::serveStatic(const char *uri, FS &fs, const char *path, const std::map<std::string, const char *> *memRes, const char *cache_header) {
    _addRequestHandler(new StaticFileRequestHandler(fs, path, uri, cache_header));
    if (memRes)
        _addRequestHandler(new StaticInMemoryRequestHandler(*memRes, uri, cache_header));
}

void HTTPServer::httpClose() {
    _state = CLOSED;
    _headersOfInterest.clear();
    for (const auto& client : _clients)
        client->close();
}

void HTTPServer::enableDelay(const bool value) {
    _nullDelay = value;
}

void HTTPServer::enableCORS(const bool value) {
    _corsEnabled = value;
}

void HTTPServer::enableCrossOrigin(const bool value) {
    enableCORS(value);
}


// void HTTPServer::collectHeaders(const char* headerKeys[], const size_t headerKeysCount) {
//     _headersReqCount = headerKeysCount + 1;
//     delete[] _currentReqHeaders;
//
//     _currentReqHeaders = new RequestArgument[_headersReqCount];
//     _currentReqHeaders[0].key = FPSTR(AUTHORIZATION_HEADER);
//     for (int i = 1; i < _headersReqCount; i++) {
//         _currentReqHeaders[i].key = headerKeys[i - 1];
//     }
// }

void HTTPServer::onFileUpload(const THandlerFunction &ufn) {
    _fileUploadHandler = ufn;
}

void HTTPServer::onNotFound(const THandlerFunction &fn) {
    _notFoundHandler = fn;
}

void HTTPServer::handleClient() {
    bool delay = _nullDelay;
    switch (_state) {
        case HANDLING_CLIENT:
            for (auto it = _clients.begin(); it != _clients.end();) {
                if (WebClient *client = *it; client->handleRequest() == HC_CLOSED) {
                    it = _clients.erase(it);
                    delete client;
                } else
                    ++it;
            }
            _state = _clients.empty() ? IDLE : HANDLING_CLIENT;
            //fall-through
        case IDLE:
            if (WiFiClient wifiClient = _server.available()) {
                bool newClient = true;
                //did we have this client before? check if same socket
                for (const auto& client : _clients) {
                    if (client->clientID() == wifiClient.socket()) {
                        newClient = false;  //same socket, so we have this client already
                        break;
                    }
                }
                if (newClient) {
                    _state = HANDLING_CLIENT;
                    if (_clients.size() >= 10) {
                        log_error("HTTPServer::handleClient() - server exceeded 10 clients and another one has arrived (IP %s, socket %d), rejecting the new client",
                            wifiClient.remoteIP().toString().c_str(), wifiClient.socket());
                        wifiClient.write(Canned503Response, strlen(Canned503Response));
                        wifiClient.stop();
                    } else {
                        _clients.push_back(new WebClient(this, wifiClient));
                        log_debug("HTTPServer::handleClient() - from IP %s through socket %d. WiFiServer state %d, total %zu clients",
                            wifiClient.remoteIP().toString().c_str(), wifiClient.socket(), _server.status(), _clients.size());
                    }
                }
            } else
                delay = delay && _state == IDLE;
            break;
        case CLOSED:
            //server is closed, no more clients handling
            //log_warn("HTTPServer::handleClient() - server is closed, no more clients handling");
            return;
    }
    if (delay)
        vTaskDelay(pdMS_TO_TICKS(100));     //this makes sense if there is nothing else going on in the task where the server runs
}

