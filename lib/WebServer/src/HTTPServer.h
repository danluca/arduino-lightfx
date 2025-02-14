/*
    HTTPServer.h - Dead simple web-server.
    Supports only one simultaneous client, knows how to handle GET and POST.

    Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.

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

#include <filesystem.h>
#include <functional>
#include <deque>
#include <map>
#include "WiFiServer.h"
#include "WebClient.h"
#include "Uri.h"

#define DEFAULT_HTTP_PORT 80
#define WEBSERVER_HAS_HOOK 1

class HTTPServer {
public:
    HTTPServer();
    virtual ~HTTPServer();

    void begin();
    void begin(uint16_t port);
    void handleClient();
    void close();
    void stop();
    void httpClose();
    [[nodiscard]] uint16_t port() const { return _port; };

    RequestHandler& on(const Uri &uri, const THandlerFunction &fn);
    RequestHandler& on(const Uri &uri, HTTPMethod method, const THandlerFunction &fn);
    RequestHandler& on(const Uri &uri, HTTPMethod method, const THandlerFunction &fn, const THandlerFunction &ufn);  //ufn handles file uploads and downloads
    bool removeRoute(const char *uri) { return removeRoute(String(uri), HTTP_ANY); }
    bool removeRoute(const char *uri, HTTPMethod method) { return removeRoute(String(uri), method); }
    bool removeRoute(const String &uri) {return removeRoute(uri, HTTP_ANY); }
    bool removeRoute(const String &uri, HTTPMethod method);
    void serveStatic(const char* uri, const SynchronizedFS& fs, const char* path, const std::map<std::string, const char*> *memRes = nullptr, const char* cache_header = nullptr);
    void serveStatic(const char* uri, FS& fs, const char* path, const std::map<std::string, const char*> *memRes = nullptr, const char* cache_header = nullptr);
    void onNotFound(const THandlerFunction &fn);  //called when handler is not assigned
    void onFileUpload(const THandlerFunction &ufn); //handle file uploads
    void addHandler(RequestHandler* handler);
    bool removeHandler(const RequestHandler *handler);
    void enableDelay(bool value = true);
    void enableCORS(bool value = true);
    void enableCrossOrigin(bool value = true);
    [[nodiscard]] bool corsEnabled() const { return _corsEnabled; }
    [[nodiscard]] bool crossOrigin() const { return _crossOrigin; }
    [[nodiscard]] bool nullDelay() const { return _nullDelay; }
    void setServerAgent(const String &agent) { _serverAgent = agent; }
    void setServerAgent(const char *agent) { _serverAgent = agent; }
    void setServerAgent(const __FlashStringHelper *agent) { _serverAgent = agent; }
    [[nodiscard]] String serverAgent() const { return _serverAgent; }
    template<typename... Args> void collectHeaders(const Args&... args) { // set the request headers to collect (variadic template version)
        _headersOfInterest.clear();
        const int argsCount = sizeof...(args);
        const String* strArgs = new String[argsCount] {String(args)...};
        _headersOfInterest.push_back(AUTHORIZATION_HEADER);
        for (int i = 0; i < argsCount; i++) {
            _headersOfInterest.push_back(strArgs[i]);
        }
        delete[] strArgs;
    }
    [[nodiscard]] const std::deque<String>& headersOfInterest() const { return _headersOfInterest; }
    // Hook
    typedef String(*ContentTypeFunction)(const String&);
    using HookFunction = std::function<bool(WebClient* client, ContentTypeFunction contentType)>;
    void addHook(const HookFunction& hook) {
        if (_hook) {
            auto previousHook = _hook;
            _hook = [previousHook, hook](WebClient* client, const ContentTypeFunction contentType) {
                const auto whatNow = previousHook(client, contentType);
                return whatNow ? hook(client, contentType) : whatNow;
            };
        } else {
            _hook = hook;
        }
    }
    enum ServerState {IDLE, HANDLING_CLIENT, CLOSED};

protected:
    void _addRequestHandler(RequestHandler* handler);
    bool _removeRequestHandler(const RequestHandler *handler);
    bool        _corsEnabled;
    bool        _nullDelay;
    bool        _crossOrigin;
    String      _serverAgent;
    std::deque<String> _headersOfInterest{};

    std::deque<RequestHandler*> _requestHandlers{};
    THandlerFunction _notFoundHandler;
    THandlerFunction _fileUploadHandler;
    HookFunction  _hook;

    std::deque<WebClient*> _clients{};
    WiFiServer _server;
    ServerState _state = IDLE;
    uint16_t _port;

    friend class WebClient;
};
