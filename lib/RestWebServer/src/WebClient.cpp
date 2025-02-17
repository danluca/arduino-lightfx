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
static const char* httpMethodStr[] = {
#define XX(num, name, string) _STR(name),
    HTTP_METHOD_MAP(XX)
#undef XX
};
#define INITIAL_HEADERS_BUFFER_SIZE 256
static constexpr size_t httpMethodsCount = std::size(httpMethodStr);
static constexpr char Content_Type[] PROGMEM = "Content-Type";
static constexpr char Content_Length[] PROGMEM = "Content-Length";
static constexpr char WWW_Authenticate[] = "WWW-Authenticate";

/**
 * Safe lookup of HTTPMethod enum from a HTTP Method name
 * @param httpName string name of the HTTP method (expected to be upper case)
 * @return the HTTPMethod enum that matches the name provided, or HTTP_ANY otherwise
 */
static HTTPMethod httpMethodFromName(const char* httpName) {
    auto method = HTTP_ANY;
    const String strHttpName(httpName);
    for (size_t i = 0; i < httpMethodsCount; i++) {
        if (strHttpName.equals(httpMethodStr[i])) {
            method = static_cast<HTTPMethod>(i);
            break;
        }
    }
    return method;
}

/**
 * Safe lookup of HTTP method name from a HTTPMethod enum.
 * @param method HTTPMethod to convert to a (name) string
 * @return name of the HTTP method to use in logging
 */
static const char* httpMethodToString(const HTTPMethod method) {
    if (method >= httpMethodsCount)
        return "HTTP UNKNOWN";
    return httpMethodStr[method];
}

/**
 * Initializes the Web Server's client wrapper for current request
 * @param server underlying WiFi server (NINA library)
 * @param client underlying WiFi client (NINA library)
 */
WebClient::WebClient(HTTPServer *server, const WiFiClient &client): _server(server), _rawWifiClient(client), _status(HC_READING),
        _requestHandler(nullptr), _contentLength(CONTENT_LENGTH_NOT_SET), _contentWritten(0), _chunked(false) {
    _request = new WebRequest();
    _startHandlingTime = millis();
    _startWaitTime = _startHandlingTime;
    _stopHandlingTime = 0;
    _rawWifiClient.setTimeout(HTTP_MAX_SEND_WAIT);
    // the ID is relying on the WiFiClient's internal socket used; the ID is used in discriminating new clients from existing ones that the WiFiServer may report
    _clientID = _rawWifiClient.socket();
    _responseHeaders.reserve(INITIAL_HEADERS_BUFFER_SIZE);
}

/**
 * Destructor - release resources
 */
WebClient::~WebClient() {
    close();
    delete _request;
    _request = nullptr;
}

/**
 * Closes the underlying connection (WiFi client) and logs the metrics of processing this request
 */
void WebClient::close() {
    if (_stopHandlingTime > 0)
        return;     //already ran
    _rawWifiClient.stop();
    _status = HC_CLOSED;
    _uploadBody.reset();
    _rawBody.reset();
    _stopHandlingTime = millis();
    log_info(F("=== Web Client ID (socket#) %d closed. Processed request %s %s in %lld ms, written %zu bytes total"), _clientID, httpMethodToString(_request->method()),
        _request->uri().c_str(), _stopHandlingTime - _startHandlingTime, _contentWritten);
}

// /**
//  * Revisit the need for this alongside the authentication method in WebRequest
//  * @param mode
//  * @param realm
//  * @param authFailMsg
//  */
// void WebClient::requestAuthentication(const HTTPAuthMethod mode, const char *realm, const String &authFailMsg) {
//     request()._sRealm = realm == nullptr ? String(F("Login Required")) : String(realm);
//     const String &sRealm = request()._sRealm;
//     if (mode == BASIC_AUTH) {
//         sendHeader(String(WWW_Authenticate), String(F("Basic realm=\"")) + sRealm + String(F("\"")));
//     } else {
//         request()._sNonce = Util::getRandomHexString();
//         request()._sOpaque = Util::getRandomHexString();
//         sendHeader(String(WWW_Authenticate),
//                    String(F("Digest realm=\"")) + sRealm + String(F("\", qop=\"auth\", nonce=\"")) + request()._sNonce +
//                    String(F("\", opaque=\"")) + request()._sOpaque + String(F("\"")));
//     }
//     send(401, String(mime::mimeTable[mime::html].mimeType), authFailMsg);
// }

/**
 * Collects a header into response headers buffer in memory. NO data is transmitted to the underlying (WiFi) client in this method.
 * @param name header name
 * @param value header value
 * @param first whether it should be the first header
 */
void WebClient::sendHeader(const String &name, const String &value, const bool first) {
    String headerLine = name;
    headerLine += F(": ");
    headerLine += value;
    headerLine += "\r\n";

    if (first)
        _responseHeaders = headerLine + _responseHeaders;
    else
        _responseHeaders += headerLine;
}

/**
 * Establishes the content-length value for the response
 * @param contentLength response content length
 */
void WebClient::setContentLength(const size_t contentLength) {
    _contentLength = contentLength;
}

/**
 * Collects all response headers and writes them to the \code response\endcode buffer. NO data is transmitted to the underlying (WiFi) client in this method.
 * @param response response buffer
 * @param code HTTP response code
 * @param content_type response content-type
 * @param contentLength response content-length
 */
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
    sendHeader(String(F("Content-Type")), String(content_type), true);
    if (_server->serverAgent().length())
        sendHeader(F("Server"), _server->serverAgent());
    if (_contentLength == CONTENT_LENGTH_NOT_SET) {
        sendHeader(String(Content_Length), String(contentLength));
        _contentLength = contentLength;
    } else {
        if (_contentLength == CONTENT_LENGTH_UNKNOWN) {
            //let's do chunked - only applicable to HTTP/1.1 or above client, i.e. all today clients
            _chunked = true;
            sendHeader(String(F("Accept-Ranges")), String(F("none")));
            sendHeader(String(F("Transfer-Encoding")), String(F("chunked")));
        } else
            sendHeader(String(Content_Length), String(_contentLength));
    }
    if (_server->corsEnabled()) {
        sendHeader(String(F("Access-Control-Allow-Origin")), String("*"));
        sendHeader(String(F("Access-Control-Allow-Methods")), String("*"));
        sendHeader(String(F("Access-Control-Allow-Headers")), String("*"));
    }
    sendHeader(String(F("Connection")), String(F("close")));

    response += _responseHeaders;
    response += "\r\n";
    log_info( F("Web Response: status code %d (%s), content type %s, length %zu"), code, Util::responseCodeToString(code).c_str(),
      content_type, _contentLength);
    log_debug(F("=== Headers ===\n%s=== Body Size %zu ===\n"), response.c_str(), _contentLength);
    _responseHeaders = "";
}

/**
 * Sends a complete response to the underlying WiFi client - this method does transmit data to the WiFi client.
 * @param code http response code
 * @param content_type response content-type
 * @param content response body content (may be empty)
 * @return number of bytes written to underlying WiFi client
 */
size_t WebClient::send(const int code, const char *content_type, const String &content) {
    String headers;
    headers.reserve(INITIAL_HEADERS_BUFFER_SIZE); //decent starting size of headers
    // Can we assume the following?
    //if(code == 200 && content.length() == 0 && _contentLength == CONTENT_LENGTH_NOT_SET)
    //  _contentLength = CONTENT_LENGTH_UNKNOWN;
    if (_contentLength == CONTENT_LENGTH_NOT_SET) {
        if (content.length() == 0) {
            log_warn("Web Response - Content length is zero or unknown (improper streaming?)");
            _contentLength = CONTENT_LENGTH_UNKNOWN;
        } else
            _contentLength = content.length();
    }
    _prepareHeader(headers, code, content_type, content.length());
    size_t contentSent = _currentClientWrite(headers.c_str(), headers.length());
    if (content.length())
        contentSent += sendContent(content);
    _contentWritten += contentSent;
    return contentSent;
}

/**
 * Sends a complete response to the underlying WiFi client - this method does transmit data to the WiFi client.
 * @param code http response code
 * @param content_type response content-type
 * @param content response body content (may be empty)
 * @return number of bytes written to underlying WiFi client
 */
size_t WebClient::send(const int code, const String &content_type, const String &content) {
    return send(code, content_type.c_str(), content);
}

/**
 * Sends a complete response to the underlying WiFi client - this method does transmit data to the WiFi client.
 * @param code http response code
 * @param content_type response content-type (may be null - defaults to text/html)
 * @param content response body content (may be null)
 * @return number of bytes written to underlying WiFi client
 */
size_t WebClient::send(const int code, const char *content_type, const char *content) {
    return send(code, content_type, content, content ? strlen(content) : 0);
}

/**
 * Sends a complete response to the underlying WiFi client - this method does transmit data to the WiFi client.
 * @param code http response code
 * @param content_type response content-type (may be null - defaults to text/html)
 * @param content response body content (may be null)
 * @param contentLength response body content length
 * @return number of bytes written to underlying WiFi client
 */
size_t WebClient::send(const int code, const char *content_type, const char *content, const size_t contentLength) {
    String headers;
    headers.reserve(INITIAL_HEADERS_BUFFER_SIZE);
    _prepareHeader(headers, code, content_type, contentLength);
    size_t contentSent = _currentClientWrite(headers.c_str(), headers.length());
    if (contentLength)
        contentSent += sendContent(content, contentLength);
    _contentWritten += contentSent;
    return contentSent;
}

size_t WebClient::send_P(const int code, PGM_P content_type, PGM_P content) {
    const size_t contentLength = content ? strlen_P(content) : 0;
    String headers;
    headers.reserve(INITIAL_HEADERS_BUFFER_SIZE);
    _prepareHeader(headers, code, content_type, contentLength);
    size_t contentSent = _currentClientWrite(headers.c_str(), headers.length());
    contentSent += sendContent_P(content, contentLength);
    _contentWritten += contentSent;
    return contentSent;
}

size_t WebClient::send_P(const int code, PGM_P content_type, PGM_P content, const size_t contentLength) {
    String headers;
    headers.reserve(INITIAL_HEADERS_BUFFER_SIZE);
    _prepareHeader(headers, code, content_type, contentLength);
    size_t contentSent = sendContent(headers);
    contentSent += sendContent_P(content, contentLength);
    _contentWritten += contentSent;
    return contentSent;
}

/**
 * Sends content to the underlying WiFi client - this method does transmit data to the WiFi client.
 * Chunks it if enabled (must be paired with proper http headers for chunking)
 * @param content content to send
 * @return number of bytes written to underlying WiFi client
 */
size_t WebClient::sendContent(const String &content) {
    return sendContent(content.c_str(), content.length());
}

/**
 * Sends content to the underlying WiFi client - this method does transmit data to the WiFi client.
 * Chunks it if enabled (must be paired with proper http headers for chunking)
 * @param content content to send
 * @param contentLength size of content to send
 * @return number of bytes written to underlying WiFi client
 */
size_t WebClient::sendContent(const char *content, const size_t contentLength) {
    const auto footer = "\r\n";
    size_t contentSent = 0;
    if (_chunked) {
        char chunkSize[11];
        sprintf(chunkSize, "%x%s", contentLength, footer);
        contentSent += _currentClientWrite(chunkSize, strlen(chunkSize));
    }
    contentSent += _currentClientWrite(content, contentLength);
    if (_chunked) {
        contentSent += _rawWifiClient.write(footer, 2);
        if (contentLength == 0) {
            _chunked = false;
        }
    }
    _contentWritten += contentSent;
    return contentSent;
}

size_t WebClient::sendContent_P(PGM_P content) {
    return sendContent_P(content, strlen_P(content));
}

size_t WebClient::sendContent_P(PGM_P content, const size_t size) {
    const auto footer = "\r\n";
    size_t contentSent = 0;
    if (_chunked) {
        char chunkSize[11];
        sprintf(chunkSize, "%x%s", size, footer);
        contentSent += _currentClientWrite(chunkSize, strlen(chunkSize));
    }
    contentSent += _currentClientWrite_P(content, size);
    if (_chunked) {
        contentSent += _rawWifiClient.write(footer, 2);
        if (size == 0)
            _chunked = false;
    }
    _contentWritten += contentSent;
    return contentSent;
}

/**
 * Sends the response headers in preparation of streaming contents of a file. The headers are transmitted to the underlying WiFi client connection.
 * @param fileSize size of file to send
 * @param fileName name of the file to send
 * @param contentType response content-type
 * @param code http response code
 * @return number of bytes written to the underlying WiFi client
 */
size_t WebClient::_streamFileCore(const size_t fileSize, const String &fileName, const String &contentType, const int code) {
    using namespace mime;
    setContentLength(fileSize);
    if (fileName.endsWith(String(mimeTable[gz].endsWith)) && contentType != String(mimeTable[gz].mimeType) &&
            contentType != String(mimeTable[none].mimeType)) {
        sendHeader(F("Content-Encoding"), F("gzip"));
    }
    return send(code, contentType, "");
}

/**
 * Finalize the response - meaningful if chunked (otherwise, really a no-op as the WiFi client flush() method does nothing)
 */
void WebClient::_finalizeResponse() {
    if (_chunked)
        sendContent("");
    _rawWifiClient.flush();
    log_info(F("========== Web response completed for request %s %s"), httpMethodToString(request().method()), request().uri().c_str());
}

/**
 * Processes the request with user supplied handlers. Equivalent with controller implementation in an MVC pattern.
 */
void WebClient::_processRequest() {
    bool handled = false;
    if (!_requestHandler)
        log_error("Web request handler not found for %s request %s", httpMethodToString(request().method()), request().uri().c_str());
    else {
        handled = _requestHandler->handle(*this);
        if (!handled)
            log_error("Web request handler failed to handle %s request %s", httpMethodToString(request().method()), request().uri().c_str());
    }
    if (!handled && _server->_notFoundHandler) {
        (_server->_notFoundHandler)(*this);
        handled = true;
    }
    if (!handled) {
        using namespace mime;
        send(404, String(mimeTable[html].mimeType), String(F("Not found: ")) + request().uri());
        handled = true;
    }
    _finalizeResponse();
}

/**
 * Parses inbound http headers into the \code WebRequest\endcode object.
 * Only headers of interest configured into the server are retained; nonetheless the entire http headers section is consumed
 * from the inbound request (WiFi client).
 */
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
        for (const auto &h: _server->_headersOfInterest) {
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

        if (headerName.equalsIgnoreCase(Content_Type)) {
            if (headerValue.startsWith(F("multipart/"))) {
                _request->_boundaryStr = headerValue.substring(headerValue.indexOf('=') + 1);
                _request->_boundaryStr.replace("\"", "");
            }
        } else if (headerName.equalsIgnoreCase(F("Content-Length")))
            _request->_contentLength = headerValue.toInt();
    }
}

bool WebClient::_handleRawData() {
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
            return false;
        }
        _requestHandler->raw(*this);
    }
    //notify the handler the raw reading has ended
    _rawBody->status = RAW_END;
    _requestHandler->raw(*this);
    log_debug(F("Raw length read %zu (client content length %d)\n====="), _rawBody->totalSize,
              request().contentLength());
    return true;
}

/**
 * Parses the HTTP request into elements to aid in processing the request. Note that traditional web form parsing is not supported;
 * for a platform limited in resources it makes more sense to engage in REST-ful calls using JSON formatted data for any form-like data updates
 * @param client WiFi client
 * @return indication of the action client handling code needs to take
 */
bool WebClient::_parseRequest() {
    // Read the first line of HTTP request
    const String req = _rawWifiClient.readStringUntil('\r');
    _rawWifiClient.readStringUntil('\n');

    // First line of HTTP request looks like "GET /path HTTP/1.1"
    // Retrieve the "/path" part by finding the spaces
    const int addr_start = req.indexOf(' ');
    const int addr_end = req.indexOf(' ', addr_start + 1);
    if (addr_start == -1 || addr_end == -1) {
        log_error("Invalid HTTP request: %s", req.c_str());
        return false;
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
    request()._contentLength = 0; // not known yet, or invalid

    if (_server->_hook && _server->_hook(this, mime::getContentType))
        return true;

    const auto method = httpMethodFromName(methodStr.c_str());
    if (method == HTTP_ANY) {
        log_error("Unknown HTTP Method: %s", methodStr.c_str());
        return false;
    }
    request()._method = method;
    request()._boundaryStr = "";

    log_debug( F("Web Request data: originating from %s; URI: %s %s %s; content length: %zu"), _rawWifiClient.remoteIP().toString().c_str(),
        methodStr.c_str(), request()._reqUrl.c_str(), searchStr.c_str(), request()._contentLength);
    _parseArguments(searchStr);
    _parseHttpHeaders();

    //attach handler
    for (const auto &reqHandler: _server->_requestHandlers) {
        if (reqHandler->canHandle(*this)) {
            _requestHandler = reqHandler;
            break;
        }
    }

    if (_requestHandler && _requestHandler->canRaw(*this)) {
        //if we can process this request as raw data, we give this option priority vs consuming the request body as a string (below)
        const bool rawAction = _handleRawData();
        _finalizeResponse();
        return rawAction;
    }
    if (request()._contentLength > HTTP_MAX_POST_DATA_LENGTH) {
        log_error(F("Web Request %s %s Content length %d exceeds maximum of %d"), methodStr.c_str(), request().uri().c_str(), request()._contentLength, HTTP_MAX_POST_DATA_LENGTH);
        _finalizeResponse();
        return false;
    }

    if (request()._contentLength > 0) {
        if (request()._method == HTTP_GET || request()._method == HTTP_HEAD)
            log_warn(F("Web Request %s %s Content length specified %d but not expected"), methodStr.c_str(), request().uri().c_str(), request()._contentLength);
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
              methodStr.c_str(), request().uri().c_str(), request()._requestBody.length(), request().contentLength());
        if (_rawWifiClient.connected()) {
            if (const int avail = _rawWifiClient.available() > 0)
                log_warn(F("Web Request %s %s Content length mismatch: read %d bytes but client still has %d bytes available"),
                    methodStr.c_str(), request().uri().c_str(), request()._requestBody.length(), avail);
        } else
            log_warn(F("Web Request %s %s read content body %d bytes but client has disconnected"),
                methodStr.c_str(), request().uri().c_str(), request()._requestBody.length());

        log_debug(F("=== Body ===\n%s====="), request()._requestBody.c_str());
    } else if (!(request().method() == HTTP_GET || request().method() == HTTP_HEAD))
        log_warn(F("Web Request %s %s Content length not specified; body - if any - ignored"), methodStr.c_str(), request().uri().c_str());
    log_info(F("===== Web Request %s %s parsed"), methodStr.c_str(), request().uri().c_str());
    return true;
}

/**
 * Parses URI query string into argument name/value pairs and stores them in request's _requestArgs queue
 * Does NOT consume data from WiFi client
 * @param data URI query string to parse
 */
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
        log_error(F("Too many arguments in request: %d; only parsing the first %d"), argCount, WEBSERVER_MAX_POST_ARGS);
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
            log_debug(F("Request arg %d key: %s, missing value - defaulting to empty string/presence"), iArg, arg->key.c_str());
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
    log_debug(F("Request args parsed %d arguments"), request()._requestArgs.size());
}

/**
 * Accumulates one more byte into the \code _uploadBody\endcode, triggers the upload method of
 * the request handler if we're going over the buffer size.
 * Note: caller is responsible to ensure the request handler can - in fact - process the request as upload
 * e.g. \code _requestHandler->canUpload(*this)\endcode, this method will not check as it becomes very inefficient.
 * Note: prefer using the buffered method overload instead, more efficient in transferring data than one byte at a time
 * @param b byte to accumulate in the uploadBody
 * @see WebClient::_uploadWriteBytes(const uint8_t *b, const size_t len)
 */
void WebClient::_uploadWriteByte(const uint8_t b) {
    if (_uploadBody->currentSize == HTTP_UPLOAD_BUFLEN) {
        _requestHandler->upload(*this);
        _uploadBody->totalSize += _uploadBody->currentSize;
        _uploadBody->currentSize = 0;
    }
    _uploadBody->buf[_uploadBody->currentSize++] = b;
}

/**
 * Accumulates a byte array into the \code _uploadBody\endcode, triggers the upload method of
 * the request handler if we're going over the buffer size.
 * Note: caller is responsible to ensure the request handler can - in fact - process the request as upload
 * e.g. \code _requestHandler->canUpload(*this)\endcode, this method will not check as it becomes very inefficient.
 * @param b byte array to accumulate in the uploadBody
 * @param len length of the byte array
 */
void WebClient::_uploadWriteBytes(const uint8_t *b, const size_t len) {
    size_t leftToWrite = len;
    while (leftToWrite > 0) {
        const size_t toWrite = min(leftToWrite, HTTP_UPLOAD_BUFLEN - _uploadBody->currentSize);
        memcpy(_uploadBody->buf + _uploadBody->currentSize, b, toWrite);
        _uploadBody->currentSize += toWrite;
        leftToWrite -= toWrite;
        if (_uploadBody->currentSize == HTTP_UPLOAD_BUFLEN) {
            _requestHandler->upload(*this);
            _uploadBody->totalSize += _uploadBody->currentSize;
            _uploadBody->currentSize = 0;
        }
    }
}

/**
 * Read a single byte from the underlying WiFi client connection, with timeout
 * Note: prefer using the buffered overload _uploadReadBytes instead, more efficient in transferring data than one byte at a time
 * @return the byte read as integer. If negative value it means we timed out reading or client disconnected
 */
int WebClient::_uploadReadByte() {
    int res = _rawWifiClient.read();

    if (res < 0) {
        // keep trying until you either read a valid byte or timeout
        const unsigned long timeoutMillis = millis() + _rawWifiClient.getTimeout();
        bool timedOut = false;
        while (!timedOut) {
            // loosely modeled after blinkWithoutDelay pattern
            while (!timedOut && !_rawWifiClient.available()) {
                if (_rawWifiClient.connected()) {
                    SchedulerClassExt::delay(5); // wait for data to become available
                    timedOut = millis() >= timeoutMillis;
                } else
                    return -1;  //we're disconnected - game over
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
            //       a simple retry strategy overcomes this which is to say the assertion is not permanent, but the
            //       reason that this works is elusive, and possibly indicative of a more subtle underlying issue
        }
    }
    return res;
}

/**
 * Read up to \code len\endcode bytes from the underlying WiFi client connection into the buffer provided, with timeout
 * @param buf byte array to fill
 * @param len max size of the byte array
 * @return number of bytes read (between 0 and len)
 */
size_t WebClient::_uploadReadBytes(uint8_t *buf, const size_t len) {
    size_t readLength = 0;
    while (readLength < len) {
        const unsigned long timeout = millis() + _rawWifiClient.getTimeout();
        int availToRead = 0;
        while ((availToRead = _rawWifiClient.available()) == 0 && timeout > millis())
            SchedulerClassExt::delay(10);
        if (!availToRead)
            break;
        const size_t toRead = min(len - readLength, availToRead);
        readLength += _rawWifiClient.readBytes(buf + readLength, toRead);
    }
    return readLength;
}

/**
 * Handles the incoming request and possible outcomes of underlying WiFi client connection. It does not return until the request is fully handled,
 * successfully or not. The intent is for server to only call this once.
 * There are wait timeouts for data availability and connection closing that are sliced in 25 and 50ms intervals, respectively - without leaving the function.
 * @return current client status - informs the server whether to keep invoking this client or dispose it
 * @see HTTPServer::handleClient
 */
HTTPClientStatus WebClient::handleRequest() {
    // disconnected is an unrecoverable state
    if (!_rawWifiClient.connected())
        _status = HC_DISCONNECTED;
    ulong startClosing = 0;
    while (_status != HC_DISCONNECTED) {
        switch (_status) {
            case HC_READING:
                if (!_rawWifiClient.available()) {
                    if (millis() - _startHandlingTime <= HTTP_MAX_DATA_WAIT)
                        SchedulerClassExt::delay(25);
                    else {
                        send(408, mime::mimeTable[mime::txt].mimeType, Util::responseCodeToString(408));
                        _status = HC_CLOSING;
                        startClosing = millis();
                        break;
                    }
                }
                // Parse the request
                if (_parseRequest())
                    _status = HC_PROCESSING;
                else {
                    //this is in response to bad inbound request - either from parsing it or handling its raw data
                    send(400, mime::mimeTable[mime::txt].mimeType, Util::responseCodeToString(400));
                    _status = HC_CLOSING;
                    startClosing = millis();
                }
                break;

            case HC_PROCESSING:
                // Process the request and send a response to the client
                _processRequest();

                // Determine next step based on connection status
                if (_rawWifiClient.connected()) {
                    _status = HC_CLOSING;
                    startClosing = millis();
                } else
                    _status = HC_DISCONNECTED;
                break;

            case HC_CLOSING:
                // Give the client a chance to close the connection (the client has initiated it) - we always send the connection: close header
                if (startClosing > 0 && millis() - startClosing <= HTTP_MAX_CLOSE_WAIT) {
                    SchedulerClassExt::delay(50);
                    _status = _rawWifiClient.connected() ? HC_CLOSING : HC_DISCONNECTED;
                } else
                    _status = HC_DISCONNECTED;
                break;

            default:
                // Handle unexpected or unsupported states
                _status = HC_DISCONNECTED; // Safely terminate the connection
                break;
        }
    }

    close(); // Clean up the client connection; transition to HC_CLOSED status
    return _status; // Return the current HTTP client status, i.e. HC_CLOSED
}
