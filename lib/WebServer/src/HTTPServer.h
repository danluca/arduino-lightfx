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
#include <memory>
#include <WiFiNINA.h>
#include <map>
#include <detail/StringStream.h>

#include "HTTP_Method.h"
#include "Uri.h"

enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
enum HTTPRawStatus { RAW_START, RAW_WRITE, RAW_END, RAW_ABORTED };
enum HTTPClientStatus { HC_NONE, HC_WAIT_READ, HC_WAIT_CLOSE };
enum HTTPAuthMethod { BASIC_AUTH, DIGEST_AUTH };

#define HTTP_DOWNLOAD_UNIT_SIZE 1436

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 1436
#endif

#ifndef HTTP_RAW_BUFLEN
#define HTTP_RAW_BUFLEN 1436
#endif

#define HTTP_MAX_DATA_WAIT 5000         //ms to wait for the client to send the request
#define HTTP_MAX_DATA_AVAILABLE_WAIT 30 //ms to wait for the client to send the request when there is another client with data available
#define HTTP_MAX_POST_WAIT 5000         //ms to wait for POST data to arrive
#define HTTP_MAX_SEND_WAIT 5000         //ms to wait for data chunk to be ACKed
#define HTTP_MAX_CLOSE_WAIT 5000        //ms to wait for the client to close the connection

#define CONTENT_LENGTH_UNKNOWN ((size_t) -1)
#define CONTENT_LENGTH_NOT_SET ((size_t) -2)

#define WEBSERVER_HAS_HOOK 1

constexpr char AUTHORIZATION_HEADER[] PROGMEM = "Authorization";

class HTTPServer;

typedef struct {
    HTTPUploadStatus status {UPLOAD_FILE_START};
    String  filename;
    String  name;
    String  type;
    size_t  totalSize {0};    // file size
    size_t  currentSize {0};  // size of data currently in buf
    uint8_t buf[HTTP_UPLOAD_BUFLEN] {};
} HTTPUpload;

typedef struct {
    HTTPRawStatus status {RAW_START};
    size_t  totalSize {0};   // content size
    size_t  currentSize {0}; // size of data currently in buf
    uint8_t buf[HTTP_RAW_BUFLEN] {};
    void    *data {nullptr};       // additional data
} HTTPRaw;

#include "detail/RequestHandler.h"

class HTTPServer {
public:
    HTTPServer();
    virtual ~HTTPServer();

    virtual void httpClose();
    bool authenticate(const char * username, const char * password);
    void requestAuthentication(HTTPAuthMethod mode = BASIC_AUTH, const char* realm = nullptr, const String& authFailMsg = String(""));

    typedef std::function<void(void)> THandlerFunction;
    typedef std::function<bool(HTTPServer &server)> FilterFunction;

    RequestHandler& on(const Uri &uri, const THandlerFunction &fn);
    RequestHandler& on(const Uri &uri, HTTPMethod method, const THandlerFunction &fn);
    RequestHandler& on(const Uri &uri, HTTPMethod method, const THandlerFunction &fn, const THandlerFunction &ufn);  //ufn handles file uploads
    bool removeRoute(const char *uri);
    bool removeRoute(const char *uri, HTTPMethod method);
    bool removeRoute(const String &uri);
    bool removeRoute(const String &uri, HTTPMethod method);
    void addHandler(RequestHandler* handler);
    bool removeHandler(const RequestHandler *handler);
    void serveStatic(const char* uri, const SynchronizedFS& fs, const char* path, const std::map<std::string, const char*> *memRes = nullptr, const char* cache_header = nullptr);
    void serveStatic(const char* uri, FS& fs, const char* path, const std::map<std::string, const char*> *memRes = nullptr, const char* cache_header = nullptr);
    void onNotFound(const THandlerFunction &fn);  //called when handler is not assigned
    void onFileUpload(const THandlerFunction &ufn); //handle file uploads
    [[nodiscard]] String uri() const {
        return _currentUri;
    }
    [[nodiscard]] HTTPMethod method() const {
        return _currentMethod;
    }
    virtual WiFiClient& client() {
        return _currentClient;
    }
    [[nodiscard]] HTTPUpload& upload() const {
        return *_currentUpload;
    }
    [[nodiscard]] HTTPRaw& raw() const {
        return *_currentRaw;
    }

    [[nodiscard]] String pathArg(unsigned int i) const; // get request path argument by number
    [[nodiscard]] String arg(const String& name) const;        // get request argument value by name
    [[nodiscard]] String arg(int i) const;              // get request argument value by number
    [[nodiscard]] String argName(int i) const;          // get request argument name by number
    [[nodiscard]] int args() const;                     // get arguments count
    [[nodiscard]] bool hasArg(const String& name) const;       // check if argument exists
    // void collectHeaders(const char* headerKeys[], size_t headerKeysCount); // set the request headers to collect
    template<typename... Args> void collectHeaders(const Args&... args) { // set the request headers to collect (variadic template version)
        delete[] _currentReqHeaders;
        _headersReqCount = sizeof...(args) + 1;
        _currentReqHeaders = new RequestArgument[_headersReqCount] {
            { .key = AUTHORIZATION_HEADER, .value = "" },
            { .key = String(args), .value = "" } ...
        };
    }

    [[nodiscard]] String header(const String& name) const;     // get request header value by name
    [[nodiscard]] String header(int i) const;           // get request header value by number
    [[nodiscard]] String headerName(int i) const;       // get request header name by number
    [[nodiscard]] int headers() const;                  // get header count
    [[nodiscard]] bool hasHeader(const String& name) const;    // check if header exists
    [[nodiscard]] int clientContentLength() const {
        return _clientContentLength;    // return "content-length" of incoming HTTP header from "_currentClient"
    }

    String hostHeader();            // get request host header if available or empty String if not

    // send response to the client
    // code - HTTP response code, can be 200 or 404
    // content_type - HTTP content type, like "text/plain" or "image/png"
    // content - actual content body
    size_t send(int code, const char *content_type = nullptr, const String &content = String(""));
    size_t send(int code, const String &content_type, const String &content);
    size_t send(int code, const char *content_type, const char *content);
    size_t send(int code, const char *content_type, const char *content, size_t contentLength);
    size_t send_P(int code, PGM_P content_type, PGM_P content);
    size_t send_P(int code, PGM_P content_type, PGM_P content, size_t contentLength);
    template<typename TypeName> void send(const int code, PGM_P content_type, TypeName content, const size_t contentLength) {
        send(code, content_type, static_cast<const char *>(content), contentLength);
    }

    void enableDelay(bool value);
    void enableCORS(bool value = true);
    void enableCrossOrigin(bool value = true);
    [[nodiscard]] String serverAgent() const { return _serverAgent; }
    void setServerAgent(const String &agent) { _serverAgent = agent; }
    void setServerAgent(const char *agent) { _serverAgent = agent; }
    void setServerAgent(const __FlashStringHelper *agent) { _serverAgent = agent; }
    void setContentLength(size_t contentLength);
    void sendHeader(const String& name, const String& value, bool first = false);
    size_t sendContent(const String &content);
    size_t sendContent(const char *content, size_t contentLength);
    size_t sendContent_P(PGM_P content);
    size_t sendContent_P(PGM_P content, size_t size);
    bool chunkedResponseModeStart_P(const int code, PGM_P content_type) {
        if (_currentHttpVersion == 0)
            // no chunk mode in HTTP/1.0
        {
            return false;
        }
        setContentLength(CONTENT_LENGTH_UNKNOWN);
        send(code, content_type, "");
        return true;
    }
    bool chunkedResponseModeStart(const int code, const char* content_type) {
        return chunkedResponseModeStart_P(code, content_type);
    }
    bool chunkedResponseModeStart(const int code, const String& content_type) {
        return chunkedResponseModeStart_P(code, content_type.c_str());
    }
    void chunkedResponseFinalize() {
        sendContent("");
    }
    template<typename T> size_t streamFile(T &file, const String& contentType, const int code = 200) {
        _streamFileCore(file.size(), file.name(), contentType, code);
        return _currentClientWrite(file);
    }
    size_t streamData(const String& data, const String& contentType, const int code = 200) {
        _streamFileCore(data.length(), "", contentType, code);
        StringStream ss(data);
        return _currentClientWrite(ss);
    }
    size_t streamData(const char* data, const size_t length, const String& contentType, const int code = 200) {
        _streamFileCore(length, "", contentType, code);
        StringStream ss(data, length);
        return _currentClientWrite(ss);
    }
    size_t streamData(const __FlashStringHelper* data, const size_t length, const String& contentType, const int code = 200) {
        _streamFileCore(length, "", contentType, code);
        const String strData(data);
        StringStream ss(strData);
        return _currentClientWrite(ss);
    }
    static String urlDecode(const String& text);

    // Hook
    enum ClientAction { CLIENT_REQUEST_CAN_CONTINUE, CLIENT_REQUEST_IS_HANDLED, CLIENT_MUST_STOP, CLIENT_IS_GIVEN };
    typedef String(*ContentTypeFunction)(const String&);
    using HookFunction = std::function<ClientAction(const String& method, const String& url, WiFiClient* client, ContentTypeFunction contentType)>;
    void addHook(const HookFunction& hook) {
        if (_hook) {
            auto previousHook = _hook;
            _hook = [previousHook, hook](const String& method, const String& url, WiFiClient* client, const ContentTypeFunction contentType) {
                const auto whatNow = previousHook(method, url, client, contentType);
                if (whatNow == CLIENT_REQUEST_CAN_CONTINUE) {
                    return hook(method, url, client, contentType);
                }
                return whatNow;
            };
        } else {
            _hook = hook;
        }
    }

protected:
    // buffered current client write - note with WiFiNINA we've seen issues writing contents larger than 4k in one call
    virtual size_t _currentClientWrite(const char* b, const size_t l) {
        StringStream ss(b, l);
        return _currentClientWrite(ss);
    }
    // buffered current client write - note with WiFiNINA we've seen issues writing contents larger than 4k in one call
    virtual size_t _currentClientWrite_P(PGM_P b, const size_t l) {
        StringStream ss(b, l);
        return _currentClientWrite(ss);
    }
    // this method employs buffering due to implementation in WiFiClient
    virtual size_t _currentClientWrite(Stream& s) {
        return _currentClient.write(s);
    }
    void _addRequestHandler(RequestHandler* handler);
    bool _removeRequestHandler(const RequestHandler *handler);
    void _handleRequest();
    void _finalizeResponse();
    void _parseHttpHeaders(WiFiClient *client, String &boundaryStr);
    ClientAction _handleRawData(WiFiClient *client);
    ClientAction _parseHandleRequest(WiFiClient* client);
    void _parseArguments(const String& data);
    static String _responseCodeToString(int code);
    void _uploadWriteByte(uint8_t b);
    void _uploadWriteBytes(const uint8_t* b, size_t len);
    int _uploadReadByte(WiFiClient* client);
    size_t _uploadReadBytes(WiFiClient* client, uint8_t* buf, size_t len);
    void _prepareHeader(String& response, int code, const char* content_type, size_t contentLength);
    bool _collectHeader(const char* headerName, const char* headerValue) const;

    size_t _streamFileCore(size_t fileSize, const String &fileName, const String &contentType, int code = 200);

    static String _getRandomHexString();
    // for extracting Auth parameters
    String _extractParam(const String& authReq, const String& param, char delimit = '"');
    void resetRequestHandling();

    struct RequestArgument {
        String key;
        String value;
    };

    bool        _corsEnabled;
    String      _serverAgent;

    HTTPClientStatus _currentStatus;

    WiFiClient  _currentClient;
    HTTPMethod  _currentMethod;
    String      _currentUrl;
    String      _currentUri;
    String      _currentHttpVersion;
    time_t      _statusChangeTime;
    bool        _nullDelay;

    size_t            _headersReqCount;
    RequestArgument* _currentReqHeaders;
    size_t           _clientContentLength;	// "Content-Length" from header of incoming POST or GET request
    String           _currentRequestBody;
    String           _currentBoundaryStr;   //we're not handling multipart/form-data, but keeping it in case we need to start parsing forms
    size_t           _currentArgCount;
    RequestArgument* _currentArgs;

    RequestHandler*  _currentHandler;
    RequestHandler*  _firstHandler;
    RequestHandler*  _lastHandler;
    THandlerFunction _notFoundHandler;
    THandlerFunction _fileUploadHandler;

    std::unique_ptr<HTTPUpload> _currentUpload{};
    std::unique_ptr<HTTPRaw>    _currentRaw{};


    size_t           _contentLength;
    String           _responseHeaders;
    String           _hostHeader;
    bool             _chunked;

    String           _snonce;  // Store nonce and opaque for future comparison
    String           _sopaque;
    String           _srealm;  // Store the Auth realm between Calls

    HookFunction     _hook;
};
