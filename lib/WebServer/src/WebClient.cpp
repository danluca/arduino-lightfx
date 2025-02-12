// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "WebClient.h"

#include <HTTPServer.h>
#include <LogProxy.h>

#include "detail/util.h"
#include "detail/mimetable.h"

#ifndef WEBSERVER_MAX_POST_ARGS
#define WEBSERVER_MAX_POST_ARGS 32
#endif

#define __STR(a) #a
#define _STR(a) __STR(a)
static const char *_http_method_str[] = {
#define XX(num, name, string) _STR(name),
    HTTP_METHOD_MAP(XX)
#undef XX
};

static constexpr char Content_Type[] PROGMEM = "Content-Type";
static constexpr char Content_Length[] PROGMEM = "Content-Length";
static constexpr char WWW_Authenticate[] = "WWW-Authenticate";

WebClient::WebClient(HTTPServer *server, const WiFiClient &client): _server(server), _rawWifiClient(client), _status(HC_WAIT_READ),
    _requestHandler(nullptr), _contentLength(CONTENT_LENGTH_NOT_SET), _chunked(false) {
    _request = new WebRequest();
    _startHandlingTime = millis();
    _rawWifiClient.setTimeout(HTTP_MAX_SEND_WAIT);
}

WebClient::~WebClient() {
    close();
    delete _request;
    _request = nullptr;
}

void WebClient::close() {
    _rawWifiClient.stop();
}

/**
 * Revisit the need for this alongside the authentication method in WebRequest
 * @param mode
 * @param realm
 * @param authFailMsg
 */
void WebClient::requestAuthentication(const HTTPAuthMethod mode, const char *realm, const String &authFailMsg) {
    request()._sRealm = realm == nullptr ? String(F("Login Required")) : String(realm);
    const String &sRealm = request()._sRealm;
    if (mode == BASIC_AUTH) {
        sendHeader(String(FPSTR (WWW_Authenticate)), String(F("Basic realm=\"")) + sRealm + String(F("\"")));
    } else {
        request()._sNonce = Util::getRandomHexString();
        request()._sOpaque = Util::getRandomHexString();
        sendHeader(String(FPSTR (WWW_Authenticate)),
                   String(F("Digest realm=\"")) + sRealm + String(F("\", qop=\"auth\", nonce=\"")) + request()._sNonce +
                   String(F("\", opaque=\"")) + request()._sOpaque + String(F("\"")));
    }
    send(401, String(FPSTR (mime::mimeTable[mime::html].mimeType)), authFailMsg);
}

void WebClient::sendHeader(const String &name, const String &value, const bool first) {
    String headerLine = name;
    headerLine += F(": ");
    headerLine += value;
    headerLine += "\r\n";

    if (first) {
        _responseHeaders = headerLine + _responseHeaders;
    } else {
        _responseHeaders += headerLine;
    }
}

void WebClient::setContentLength(const size_t contentLength) {
    _contentLength = contentLength;
}

void WebClient::_prepareHeader(String &response, const int code, const char *content_type, const size_t contentLength) {
    response = String(F("HTTP/")) + request().httpVersion() + ' ';
    response += String(code);
    response += ' ';
    response += Util::responseCodeToString(code);
    response += "\r\n";

    using namespace mime;
    if (!content_type) {
        content_type = mimeTable[html].mimeType;
    }
    sendHeader(String(F("Content-Type")), String(FPSTR (content_type)), true);
    if (_server->serverAgent().length())
        sendHeader(F("Server"), _server->serverAgent());
    if (_contentLength == CONTENT_LENGTH_NOT_SET) {
        sendHeader(String(FPSTR (Content_Length)), String(contentLength));
    } else {
        if (_contentLength == CONTENT_LENGTH_UNKNOWN) {
            //let's do chunked - only applicable to HTTP/1.1 or above client, i.e. all today clients
            _chunked = true;
            sendHeader(String(F("Accept-Ranges")), String(F("none")));
            sendHeader(String(F("Transfer-Encoding")), String(F("chunked")));
        } else
            sendHeader(String(FPSTR (Content_Length)), String(_contentLength));
    }
    if (_server->corsEnabled()) {
        sendHeader(String(FPSTR ("Access-Control-Allow-Origin")), String("*"));
        sendHeader(String(FPSTR ("Access-Control-Allow-Methods")), String("*"));
        sendHeader(String(FPSTR ("Access-Control-Allow-Headers")), String("*"));
    }
    sendHeader(String(F("Connection")), String(F("close")));

    response += _responseHeaders;
    response += "\r\n";
    log_info(
        F("Web Response: status code %d (%s), content type %s, length %zu"), code,
        Util::responseCodeToString(code).c_str(), content_type, _contentLength);
    log_debug(F("=== Headers ===\n%s"), response.c_str());
    _responseHeaders = "";
}

size_t WebClient::send(const int code, const char *content_type, const String &content) {
    String headers;
    headers.reserve(256); //decent starting size of headers
    // Can we assume the following?
    //if(code == 200 && content.length() == 0 && _contentLength == CONTENT_LENGTH_NOT_SET)
    //  _contentLength = CONTENT_LENGTH_UNKNOWN;
    if (content.length() == 0 && _contentLength == CONTENT_LENGTH_NOT_SET) {
        log_warn("content length is zero or unknown (improper streaming?)");
        _contentLength = CONTENT_LENGTH_UNKNOWN;
    }
    _prepareHeader(headers, code, content_type, content.length());
    _currentClientWrite(headers.c_str(), headers.length());
    if (content.length()) {
        sendContent(content);
    }
    return headers.length() + content.length();
}

size_t WebClient::send(const int code, const String &content_type, const String &content) {
    return send(code, content_type.c_str(), content);
}

size_t WebClient::send(const int code, const char *content_type, const char *content) {
    return send(code, content_type, content, content ? strlen(content) : 0);
}

size_t WebClient::send(const int code, const char *content_type, const char *content, const size_t contentLength) {
    String headers;
    headers.reserve(256);
    _prepareHeader(headers, code, content_type, contentLength);
    _currentClientWrite(headers.c_str(), headers.length());
    if (contentLength) {
        sendContent(content, contentLength);
    }
    return headers.length() + contentLength;
}

size_t WebClient::send_P(const int code, PGM_P content_type, PGM_P content) {
    size_t contentLength = 0;

    if (content != nullptr) {
        contentLength = strlen_P(content);
    }

    String headers;
    headers.reserve(256);
    _prepareHeader(headers, code, content_type, contentLength);
    _currentClientWrite(headers.c_str(), headers.length());
    return sendContent_P(content) + headers.length();
}

size_t WebClient::send_P(const int code, PGM_P content_type, PGM_P content, const size_t contentLength) {
    String headers;
    headers.reserve(256);
    _prepareHeader(headers, code, content_type, contentLength);
    sendContent(headers);
    return sendContent_P(content, contentLength) + headers.length();
}

size_t WebClient::sendContent(const String &content) {
    return sendContent(content.c_str(), content.length());
}

size_t WebClient::sendContent(const char *content, const size_t contentLength) {
    const auto footer = "\r\n";
    if (_chunked) {
        char chunkSize[11];
        sprintf(chunkSize, "%x%s", contentLength, footer);
        _currentClientWrite(chunkSize, strlen(chunkSize));
    }
    _currentClientWrite(content, contentLength);
    if (_chunked) {
        _rawWifiClient.write(footer, 2);
        if (contentLength == 0) {
            _chunked = false;
        }
    }
    return contentLength;
}

size_t WebClient::sendContent_P(PGM_P content) {
    return sendContent_P(content, strlen_P(content));
}

size_t WebClient::sendContent_P(PGM_P content, const size_t size) {
    const auto footer = "\r\n";
    if (_chunked) {
        char chunkSize[11];
        sprintf(chunkSize, "%x%s", size, footer);
        _currentClientWrite(chunkSize, strlen(chunkSize));
    }
    _currentClientWrite_P(content, size);
    if (_chunked) {
        _rawWifiClient.write(footer, 2);
        if (size == 0) {
            _chunked = false;
        }
    }
    return size;
}


size_t WebClient::_streamFileCore(const size_t fileSize, const String &fileName, const String &contentType, const int code) {
    using namespace mime;
    setContentLength(fileSize);
    if (fileName.endsWith(String(FPSTR (mimeTable[gz].endsWith))) && contentType != String(
            FPSTR (mimeTable[gz].mimeType)) &&
        contentType != String(FPSTR (mimeTable[none].mimeType))) {
        sendHeader(F("Content-Encoding"), F("gzip"));
    }
    return send(code, contentType, "");
}

void WebClient::_finalizeResponse() {
    if (_chunked) {
        sendContent("");
    }
    log_info(F("====="));
}

void WebClient::_processRequest() {
    bool handled = false;
    if (!_requestHandler) {
        log_error("Web request handler not found for %d request %s", request().method(), request().uri().c_str());
    } else {
        handled = _requestHandler->handle(*this);
        if (!handled) {
            log_error("Web request handler failed to handle %d request %s", request().method(),
                      request().uri().c_str());
        }
    }
    if (!handled && _server->_notFoundHandler) {
        (_server->_notFoundHandler)(*this);
        handled = true;
    }
    if (!handled) {
        using namespace mime;
        send(404, String(FPSTR (mimeTable[html].mimeType)), String(F("Not found: ")) + request().uri());
        handled = true;
    }
    _finalizeResponse();
}

void WebClient::_parseHttpHeaders() {
    log_debug(F("=== Headers ==="));
    while (true) {
        String req = _rawWifiClient.readStringUntil('\r');
        _rawWifiClient.readStringUntil('\n');
        if (req.length() == 0) {
            break; //no more headers
        }
        const int headerDiv = req.indexOf(':');
        if (headerDiv == -1) {
            log_error("Invalid header: %s (ignored)", req.c_str());
            continue;
        }
        String headerName = req.substring(0, headerDiv);
        headerName.trim();
        String headerValue = req.substring(headerDiv + 1);
        headerValue.trim();
        bool hdCollected = false;
        for (const auto &h: _server->headersOfInterest()) {
            if (h.equalsIgnoreCase(headerName)) {
                auto header = new NameValuePair();
                header->key = headerName;
                header->value = headerValue;
                _request->_headers.push_back(header);
                hdCollected = true;
                break;
            }
        }
        log_debug(F("%s%s: %s"), hdCollected ? "" : "!", headerName.c_str(), headerValue.c_str());

        if (headerName.equalsIgnoreCase(FPSTR (Content_Type))) {
            if (headerValue.startsWith(F("multipart/"))) {
                _request->_boundaryStr = headerValue.substring(headerValue.indexOf('=') + 1);
                _request->_boundaryStr.replace("\"", "");
            }
        } else if (headerName.equalsIgnoreCase(F("Content-Length")))
            _request->_contentLength = headerValue.toInt();
    }
}

WebClient::ClientAction WebClient::_handleRawData() {
    log_debug(F("=== Body Parse raw ==="));
    _rawBody.reset(new HTTPRaw());
    _rawBody->status = RAW_START;
    _rawBody->totalSize = 0;
    _rawBody->currentSize = 0;
    _requestHandler->raw(*this);
    _rawBody->status = RAW_WRITE;

    while (_rawBody->totalSize < request().contentLength()) {
        _rawBody->currentSize = _rawWifiClient.readBytes(_rawBody->buf, HTTP_RAW_BUFLEN);
        _rawBody->totalSize += _rawBody->currentSize;
        if (_rawBody->currentSize == 0) {
            _rawBody->status = RAW_ABORTED;
            _requestHandler->raw(*this);
            return CLIENT_MUST_STOP;
        }
        _requestHandler->raw(*this);
    }
    //notify the handler the raw reading has ended
    _rawBody->status = RAW_END;
    _requestHandler->raw(*this);
    log_debug(F("Raw length read %zu (client content length %d)\n====="), _rawBody->totalSize,
              request().contentLength());
    return CLIENT_REQUEST_IS_HANDLED;
}

/**
 * Parses the HTTP request into elements to aid in processing the request. Note that traditional web form parsing is not supported;
 * for a platform limited in resources it makes more sense to engage in REST-ful calls using JSON formatted data for any form-like data updates
 * @param client WiFi client
 * @return indication of the action client handling code needs to take
 */
WebClient::ClientAction WebClient::_parseRequest() {
    // Read the first line of HTTP request
    const String req = _rawWifiClient.readStringUntil('\r');
    _rawWifiClient.readStringUntil('\n');

    // First line of HTTP request looks like "GET /path HTTP/1.1"
    // Retrieve the "/path" part by finding the spaces
    const int addr_start = req.indexOf(' ');
    const int addr_end = req.indexOf(' ', addr_start + 1);
    if (addr_start == -1 || addr_end == -1) {
        log_error("Invalid HTTP request: %s", req.c_str());
        return CLIENT_MUST_STOP;
    }

    const String methodStr = req.substring(0, addr_start);
    request()._reqUrl = req.substring(addr_start + 1, addr_end);
    request()._httpVersion = req.substring(addr_end + 6);
    String searchStr = "";
    if (const int hasSearch = request().url().indexOf('?'); hasSearch != -1) {
        searchStr = request().url().substring(hasSearch + 1);
        request()._reqUri = request().url().substring(0, hasSearch);
    } else
        request()._reqUri = request()._reqUrl;
    _chunked = false;
    request()._contentLength = 0; // not known yet, or invalid

    if (_server->_hook) {
        if (const auto whatNow = _server->_hook(this, mime::getContentType); whatNow != CLIENT_REQUEST_CAN_CONTINUE) {
            return whatNow;
        }
    }

    auto method = HTTP_ANY;
    constexpr size_t num_methods = sizeof(_http_method_str) / sizeof(const char *);
    for (size_t i = 0; i < num_methods; i++) {
        if (methodStr == _http_method_str[i]) {
            method = static_cast<HTTPMethod>(i);
            break;
        }
    }
    if (method == HTTP_ANY) {
        log_error("Unknown HTTP Method: %s", methodStr.c_str());
        return CLIENT_MUST_STOP;
    }
    request()._method = method;

    log_debug(
        F("Web Request data: originating from %s; URI: %s %s %s; content length: %zu"),
        _rawWifiClient.remoteIP().toString().c_str(), methodStr.c_str(), request()._reqUrl.c_str(),
        searchStr.c_str(), request()._contentLength);

    //attach handler
    for (const auto &reqHandler: _server->_requestHandlers) {
        if (reqHandler->canHandle(*this)) {
            _requestHandler = reqHandler;
            break;
        }
    }

    request()._boundaryStr = "";
    _parseHttpHeaders();
    _parseArguments(searchStr);

    if (_requestHandler && _requestHandler->canRaw(*this)) {
        const ClientAction rawAction = _handleRawData();
        log_debug(F("====="));
        _rawWifiClient.flush();
        return rawAction;
    } else if (request()._contentLength > HTTP_MAX_POST_DATA_LENGTH) {
        log_error(F("Web Request %s %s Content length %d exceeds maximum of %d"), methodStr, request().uri().c_str(), request()._contentLength, HTTP_MAX_POST_DATA_LENGTH);
        log_debug(F("====="));
        return CLIENT_MUST_STOP;
    }

    if (request()._contentLength > 0) {
        if (request()._method == HTTP_GET || request()._method == HTTP_HEAD)
            log_warn(F("Web Request %s %s Content length specified %d but not expected"), methodStr, request().uri().c_str(), request()._contentLength);
        size_t leftToRead = request()._contentLength;
        request()._requestBody.reserve(request()._contentLength);
        while (_rawWifiClient.connected() && leftToRead > 0) {
            const auto plainBuf = new char[HTTP_RAW_BUFLEN + 1];
            const size_t lengthRead = Util::readBytesWithTimeout(&_rawWifiClient, plainBuf, min(leftToRead, HTTP_RAW_BUFLEN), HTTP_MAX_POST_WAIT);
            plainBuf[lengthRead] = '\0';
            request()._requestBody += plainBuf;
            leftToRead -= lengthRead;
        }
        if (request()._requestBody.length() != request()._contentLength)
            log_warn(F("Web Request %s %s Content length mismatch: read %d != header %d"),
              methodStr, request().uri().c_str(), request()._requestBody.length(), request().contentLength());

        log_debug(F("=== Body ===\n%s====="), request()._requestBody.c_str());
    } else if (!(request().method() == HTTP_GET || request().method() == HTTP_HEAD))
        log_warn(F("Web Request %s %s Content length not specified; body - if any - ignored"), methodStr, request().uri().c_str());
    log_debug(F("====="));
    _rawWifiClient.flush();
    return CLIENT_REQUEST_CAN_CONTINUE;
}

void WebClient::_parseArguments(const String &data) const {
    log_debug("Request args: %s", data.c_str());
    if (data.length() == 0) {
        return;
    }
    int argCount = 1; //we have at least 1 arg if the search data string has any length
    for (int i = 0; i < data.length();) {
        i = data.indexOf('&', i + 1);
        if (i == -1)
            break;
        argCount++;
    }
    if (argCount > WEBSERVER_MAX_POST_ARGS) {
        log_error("Too many arguments in request: %d; only parsing the first %d", argCount, WEBSERVER_MAX_POST_ARGS);
        argCount = WEBSERVER_MAX_POST_ARGS;
    }
    int pos = 0;
    for (int iArg = 0; iArg < argCount; iArg++) {
        const int equal_sign_index = data.indexOf('=', pos);
        const int next_arg_index = data.indexOf('&', pos);
        if (equal_sign_index < 0 || (equal_sign_index > next_arg_index && next_arg_index > -1)) {
            const auto arg = new NameValuePair();
            arg->key = Uri::urlDecode(next_arg_index < 0 ? data.substring(pos) : data.substring(pos, next_arg_index));
            arg->value = "";
            request()._requestArgs.push_back(arg);
            log_debug("Request arg %d key: %s, missing value - defaulting to empty string/presence", iArg, arg->key.c_str());
            if (next_arg_index < 0)
                break;
            pos = next_arg_index + 1;
            continue;
        }
        const auto arg = new NameValuePair();
        arg->key = Uri::urlDecode(data.substring(pos, equal_sign_index));
        arg->value = Uri::urlDecode(data.substring(equal_sign_index + 1, next_arg_index));
        log_debug("Request arg %d key: %s value: %s", iArg, arg->key.c_str(), arg->value.c_str());
        if (next_arg_index < 0)
            break;
        pos = next_arg_index + 1;
    }
    log_debug("Request args parsed %d arguments", request()._requestArgs.size());
}

//Update this to write a buffer not one byte at a time
void WebClient::_uploadWriteByte(const uint8_t b) {
    if (_uploadBody->currentSize == HTTP_UPLOAD_BUFLEN) {
        if (_requestHandler && _requestHandler->canUpload(*this)) {
            _requestHandler->upload(*this);
        }
        _uploadBody->totalSize += _uploadBody->currentSize;
        _uploadBody->currentSize = 0;
    }
    _uploadBody->buf[_uploadBody->currentSize++] = b;
}

void WebClient::_uploadWriteBytes(const uint8_t *b, const size_t len) {
    size_t leftToWrite = len;
    while (leftToWrite > 0) {
        const size_t toWrite = min(leftToWrite, HTTP_UPLOAD_BUFLEN - _uploadBody->currentSize);
        memcpy(_uploadBody->buf + _uploadBody->currentSize, b, toWrite);
        _uploadBody->currentSize += toWrite;
        leftToWrite -= toWrite;
        if (_uploadBody->currentSize == HTTP_UPLOAD_BUFLEN) {
            if (_requestHandler && _requestHandler->canUpload(*this)) {
                _requestHandler->upload(*this);
            }
            _uploadBody->totalSize += _uploadBody->currentSize;
            _uploadBody->currentSize = 0;
        }
    }
}

//Update this to read buffered not one byte at a time
int WebClient::_uploadReadByte() {
    int res = _rawWifiClient.read();

    if (res < 0) {
        // keep trying until you either read a valid byte or timeout
        const unsigned long timeoutMillis = millis() + _rawWifiClient.getTimeout();
        bool timedOut = false;
        for (;;) {
            if (!_rawWifiClient.connected()) {
                return -1;
            }
            // loosely modeled after blinkWithoutDelay pattern
            while (!timedOut && !_rawWifiClient.available() && _rawWifiClient.connected()) {
                delay(2);
                timedOut = millis() >= timeoutMillis;
            }

            res = _rawWifiClient.read();
            if (res >= 0) {
                return res; // exit on a valid read
            }
            // NOTE: it is possible to get here and have all of the following
            //       assertions hold true
            //
            //       -- client.available() > 0
            //       -- client.connected == true
            //       -- res == -1
            //
            //       a simple retry strategy overcomes this which is to say the
            //       assertion is not permanent, but the reason that this works
            //       is elusive, and possibly indicative of a more subtle underlying
            //       issue

            timedOut = millis() >= timeoutMillis;
            if (timedOut)
                return res; // exit on a timeout
        }
    }
    return res;
}

size_t WebClient::_uploadReadBytes(uint8_t *buf, const size_t len) {
    size_t readLength = 0;
    while (readLength < len) {
        const unsigned long timeout = millis() + _rawWifiClient.getTimeout();
        int availToRead = 0;
        while ((availToRead = _rawWifiClient.available()) == 0 && timeout > millis())
            delay(10);
        if (!availToRead)
            break;
        const size_t toRead = min(len - readLength, availToRead);
        readLength += _rawWifiClient.readBytes(buf + readLength, toRead);
    }
    return readLength;
}

HTTPClientStatus WebClient::handleRequest() {
    bool keepClient = false;
    if (_rawWifiClient.connected()) {
        switch (_status) {
            case HC_WAIT_READ:
                // Wait for data from client to become available
                if (_rawWifiClient.available()) {
                    switch (_parseRequest()) {
                        case CLIENT_REQUEST_CAN_CONTINUE:
                            _contentLength = CONTENT_LENGTH_NOT_SET;
                            _processRequest();
                        /* fallthrough */
                        case CLIENT_REQUEST_IS_HANDLED:
                            if (_rawWifiClient.connected() || _rawWifiClient.available()) {
                                _status = HC_WAIT_CLOSE;
                                _startHandlingTime = millis();
                                keepClient = true;
                            } else {
                                //Log.debug("webserver: peer has closed after served\n");
                            }
                            break;
                        case CLIENT_MUST_STOP:
                            //this is in response to bad inbound request - either from parsing it or handling its raw data
                            send(400, mime::mimeTable[mime::txt].mimeType, Util::responseCodeToString(400));
                            _rawWifiClient.stop();
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
                    if (const unsigned long timeSinceChange = millis() - _startHandlingTime;
                        timeSinceChange > HTTP_MAX_DATA_WAIT) {
                        // log_debug("WebServer: closing after read timeout %d ms", HTTP_MAX_DATA_WAIT);
                    } else
                        keepClient = true;
                }
                break;
            case HC_WAIT_CLOSE:
                // Wait for client to close the connection
                if (millis() - _startHandlingTime <= HTTP_MAX_CLOSE_WAIT)
                    keepClient = true;
                break;
            case HC_COMPLETED:
                log_warn("WebClient marked completed but WiFiClient still connected; closing it");
                break;
        }
    }

    if (!keepClient) {
        _rawWifiClient.stop();
        _status = HC_COMPLETED;
        _uploadBody.reset();
        _rawBody.reset();
    }
    return _status;
}

