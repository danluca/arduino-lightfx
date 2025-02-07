/*
    Parsing.cpp - HTTP request parsing.

    Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.

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
#include "WiFiServer.h"
#include "WiFiClient.h"
#include "HTTPServer.h"
#include "detail/mimetable.h"
#include "../../PicoLog/src/LogProxy.h"

#ifndef WEBSERVER_MAX_POST_ARGS
#define WEBSERVER_MAX_POST_ARGS 32
#endif

#define __STR(a) #a
#define _STR(a) __STR(a)
static const char * _http_method_str[] = {
#define XX(num, name, string) _STR(name),
    HTTP_METHOD_MAP(XX)
#undef XX
};

static constexpr char Content_Type[] PROGMEM = "Content-Type";

/**
 * Read maximum number of characters specified, engaging a timeout for data to become available from client for each read attempt
 * @param client current WiFi client
 * @param buffer buffer to hold read data
 * @param bufLength length of the buffer (max number of bytes to read)
 * @param timeout_ms amount of time to wait for data to be available per read attempt
 * @return number of bytes/chars read
 */
static size_t readBytesWithTimeout(WiFiClient* client, char* buffer, const size_t bufLength, const int timeout_ms) {
    size_t dataLength = 0;
    while (dataLength < bufLength) {
        const time_t timeout = millis() + timeout_ms;
        int availToRead = 0;
        while ((availToRead = client->available()) == 0 && timeout > millis())
            delay(10);
        if (!availToRead)
            break;
        const size_t toRead = min(bufLength - dataLength, availToRead);
        dataLength += client->readBytes(buffer + dataLength, toRead);
    }
    return dataLength;
}

void HTTPServer::_parseHttpHeaders(WiFiClient *client, String &boundaryStr) {
    log_debug(F("=== Headers ==="));
    while (true) {
        String req = client->readStringUntil('\r');
        client->readStringUntil('\n');
        if (req.length() == 0) {
            break;    //no more headers
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
        bool hdCollected = _collectHeader(headerName.c_str(), headerValue.c_str());
        log_debug(F("%c%s: %s"), hdCollected ? '':'!', headerName.c_str(), headerValue.c_str());

        if (headerName.equalsIgnoreCase(FPSTR(Content_Type))) {
            if (headerValue.startsWith(F("multipart/"))) {
                boundaryStr = headerValue.substring(headerValue.indexOf('=') + 1);
                boundaryStr.replace("\"", "");
            }
        } else if (headerName.equalsIgnoreCase(F("Content-Length")))
            _clientContentLength = headerValue.toInt();
        else if (headerName.equalsIgnoreCase(F("Host")))
            _hostHeader = headerValue;
    }
}

HTTPServer::ClientAction HTTPServer::_handleRawData(WiFiClient *client) {
    log_debug(F("=== Body Parse raw ==="));
    _currentRaw.reset(new HTTPRaw());
    _currentRaw->status = RAW_START;
    _currentRaw->totalSize = 0;
    _currentRaw->currentSize = 0;
    _currentHandler->raw(*this, _currentUri, *_currentRaw);
    _currentRaw->status = RAW_WRITE;

    while (_currentRaw->totalSize < _clientContentLength) {
        _currentRaw->currentSize = client->readBytes(_currentRaw->buf, HTTP_RAW_BUFLEN);
        _currentRaw->totalSize += _currentRaw->currentSize;
        if (_currentRaw->currentSize == 0) {
            _currentRaw->status = RAW_ABORTED;
            _currentHandler->raw(*this, _currentUri, *_currentRaw);
            return CLIENT_MUST_STOP;
        }
        _currentHandler->raw(*this, _currentUri, *_currentRaw);
    }
    //notify the handler the raw reading has ended
    _currentRaw->status = RAW_END;
    _currentHandler->raw(*this, _currentUri, *_currentRaw);
    log_debug(F("Raw length read %zu (client content length %d)\n====="), _currentRaw->totalSize, _clientContentLength);
    return CLIENT_REQUEST_IS_HANDLED;
}

/**
 * Parses the HTTP request into elements to aid in processing the request. Note that traditional web form parsing is not supported;
 * for a platform limited in resources it makes more sense to engage in REST-ful calls using JSON formatted data for any form-like data updates
 * @param client WiFi client
 * @return indication of the action client handling code needs to take
 */
HTTPServer::ClientAction HTTPServer::_parseHandleRequest(WiFiClient* client) {
    // Read the first line of HTTP request
    const String req = client->readStringUntil('\r');
    client->readStringUntil('\n');
    //reset header value
    for (int i = 0; i < _headersReqCount; ++i) {
        _currentReqHeaders[i].value = String();
        _currentReqHeaders[i].value.reserve(64);
    }

    // First line of HTTP request looks like "GET /path HTTP/1.1"
    // Retrieve the "/path" part by finding the spaces
    const int addr_start = req.indexOf(' ');
    const int addr_end = req.indexOf(' ', addr_start + 1);
    if (addr_start == -1 || addr_end == -1) {
        log_error("Invalid HTTP request: %s", req.c_str());
        return CLIENT_MUST_STOP;
    }

    const String methodStr = req.substring(0, addr_start);
    _currentUrl = req.substring(addr_start + 1, addr_end);
    _currentHttpVersion = req.substring(addr_end + 6);
    String searchStr = "";
    if (const int hasSearch = _currentUrl.indexOf('?'); hasSearch != -1) {
        searchStr = _currentUrl.substring(hasSearch + 1);
        _currentUri = _currentUrl.substring(0, hasSearch);
    } else
        _currentUri = _currentUrl;
    _chunked = false;
    _clientContentLength = 0;  // not known yet, or invalid

    if (_hook) {
        if (const auto whatNow = _hook(methodStr, _currentUri, client, mime::getContentType); whatNow != CLIENT_REQUEST_CAN_CONTINUE) {
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
    _currentMethod = method;

    log_debug(F("Web Request data: URI: %s [%d] %s %s; content length: %d"), methodStr.c_str(), _currentMethod, url.c_str(), searchStr.c_str(), _clientContentLength);

    //attach handler
    RequestHandler* handler;
    for (handler = _firstHandler; handler; handler = handler->next()) {
        if (handler->canHandle(*this, _currentMethod, _currentUri)) {
            break;
        }
    }
    _currentHandler = handler;

    _currentBoundaryStr = "";
    _parseHttpHeaders(client, _currentBoundaryStr);
    _parseArguments(searchStr);

    if (_currentHandler && _currentHandler->canRaw(*this, _currentUri)) {
        const ClientAction rawAction = _handleRawData(client);
        log_debug(F("====="));
        client->flush();
        return rawAction;
    }

    if (_contentLength > 0) {
        if ( _currentMethod == HTTP_GET || _currentMethod == HTTP_HEAD)
            log_warn(F("Web Request %s %s Content length specified %d but not expected"), methodStr, _currentUri.c_str(), _contentLength);
        size_t leftToRead = _contentLength;
        _currentRequestBody.reserve(_contentLength);
        while (client->connected() && leftToRead > 0) {
            const auto plainBuf = new char[HTTP_RAW_BUFLEN+1];
            const size_t lengthRead = readBytesWithTimeout(client, plainBuf, min(leftToRead, HTTP_RAW_BUFLEN), HTTP_MAX_POST_WAIT);
            plainBuf[lengthRead] = '\0';
            _currentRequestBody += plainBuf;
            leftToRead -= lengthRead;
        }
        if (_currentRequestBody.length() != _contentLength)
            log_warn(F("Web Request %s %s Content length mismatch: read %d != header %d"), methodStr, _currentUri.c_str(), _currentRequestBody.length(), _contentLength);

        log_debug(F("=== Body ===\n%s====="), _currentRequestBody.c_str());
    } else if (!(_currentMethod == HTTP_GET || _currentMethod == HTTP_HEAD))
        log_warn(F("Web Request %s %s Content length not specified; body - if any - ignored"), methodStr, _currentUri.c_str());
    log_debug(F("====="));
    client->flush();
    return CLIENT_REQUEST_CAN_CONTINUE;
}

bool HTTPServer::_collectHeader(const char* headerName, const char* headerValue) const {
    for (int i = 0; i < _headersReqCount; i++) {
        if (_currentReqHeaders[i].key.equalsIgnoreCase(headerName)) {
            _currentReqHeaders[i].value = headerValue;
            return true;
        }
    }
    return false;
}

void HTTPServer::_parseArguments(const String& data) {
    log_debug("Request args: %s", data.c_str());
    delete[] _currentArgs;
    _currentArgs = nullptr;
    if (data.length() == 0) {
        _currentArgCount = 0;
        return;
    }
    _currentArgCount = 1;   //we have at least 1 arg if the search data string has any length
    for (int i = 0; i < data.length();) {
        i = data.indexOf('&', i+1);
        if (i == -1)
            break;
        _currentArgCount++;
    }
    if (_currentArgCount > WEBSERVER_MAX_POST_ARGS) {
        log_error("Too many arguments in request: %d; only parsing the first %d", _currentArgCount, WEBSERVER_MAX_POST_ARGS);
        _currentArgCount = WEBSERVER_MAX_POST_ARGS;
    }
    _currentArgs = new RequestArgument[_currentArgCount];
    int pos = 0;
    int iArg;
    for (iArg = 0; iArg < _currentArgCount;) {
        const int equal_sign_index = data.indexOf('=', pos);
        const int next_arg_index = data.indexOf('&', pos);
        if (equal_sign_index < 0 || (equal_sign_index > next_arg_index && next_arg_index > -1)) {
            log_debug("Request arg %d missing value, default to empty string/presence", iarg);
            auto &[key, value] = _currentArgs[iArg++];
            key = urlDecode(next_arg_index < 0 ? data.substring(pos) : data.substring(pos, next_arg_index));
            value = "";
            if (next_arg_index < 0)
                break;
            pos = next_arg_index + 1;
            continue;
        }
        auto &[key, value] = _currentArgs[iArg++];
        key = urlDecode(data.substring(pos, equal_sign_index));
        value = urlDecode(data.substring(equal_sign_index + 1, next_arg_index));
        log_debug("Request arg %d key: %s value: %s", iarg, arg.key.c_str(), arg.value.c_str());
        if (next_arg_index < 0)
            break;
        pos = next_arg_index + 1;
    }
    _currentArgCount = iArg;
    log_debug("Request args parsed %d arguments", _currentArgCount);

}

//Update this to write a buffer not one byte at a time
void HTTPServer::_uploadWriteByte(const uint8_t b) {
    if (_currentUpload->currentSize == HTTP_UPLOAD_BUFLEN) {
        if (_currentHandler && _currentHandler->canUpload(*this, _currentUri)) {
            _currentHandler->upload(*this, _currentUri, *_currentUpload);
        }
        _currentUpload->totalSize += _currentUpload->currentSize;
        _currentUpload->currentSize = 0;
    }
    _currentUpload->buf[_currentUpload->currentSize++] = b;
}

void HTTPServer::_uploadWriteBytes(const uint8_t* b, const size_t len) {
    size_t leftToWrite = len;
    while (leftToWrite > 0) {
        const size_t toWrite = min(leftToWrite, HTTP_UPLOAD_BUFLEN - _currentUpload->currentSize);
        memcpy(_currentUpload->buf + _currentUpload->currentSize, b, toWrite);
        _currentUpload->currentSize += toWrite;
        leftToWrite -= toWrite;
        if (_currentUpload->currentSize == HTTP_UPLOAD_BUFLEN) {
            if (_currentHandler && _currentHandler->canUpload(*this, _currentUri)) {
                _currentHandler->upload(*this, _currentUri, *_currentUpload);
            }
            _currentUpload->totalSize += _currentUpload->currentSize;
            _currentUpload->currentSize = 0;
        }
    }
}

//Update this to read buffered not one byte at a time
int HTTPServer::_uploadReadByte(WiFiClient * client) {
    int res = client->read();

    if (res < 0) {
        // keep trying until you either read a valid byte or timeout
        const unsigned long timeoutMillis = millis() + client->getTimeout();
        bool timedOut = false;
        for (;;) {
            if (!client->connected()) {
                return -1;
            }
            // loosely modeled after blinkWithoutDelay pattern
            while (!timedOut && !client->available() && client->connected()) {
                delay(2);
                timedOut = millis() >= timeoutMillis;
            }

            res = client->read();
            if (res >= 0) {
                return res;  // exit on a valid read
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
                return res;  // exit on a timeout
        }
    }
    return res;
}

size_t HTTPServer::_uploadReadBytes(WiFiClient* client, uint8_t* buf, const size_t len) {
    size_t readLength = 0;
    while (readLength < len) {
        const unsigned long timeout = millis() + client->getTimeout();
        int availToRead = 0;
        while ((availToRead = client->available()) == 0 && timeout > millis())
            delay(10);
        if (!availToRead)
            break;
        const size_t toRead = min(len - readLength, availToRead);
        readLength += client->readBytes(buf + readLength, toRead);
    }
    return readLength;
}

String HTTPServer::urlDecode(const String & text) {
    String decoded = "";
    char temp[] = "0x00";
    const unsigned int len = text.length();
    unsigned int i = 0;
    while (i < len) {
        char decodedChar;
        if (const char encodedChar = text.charAt(i++); (encodedChar == '%') && (i + 1 < len)) {
            temp[2] = text.charAt(i++);
            temp[3] = text.charAt(i++);

            decodedChar = strtol(temp, nullptr, 16);
        } else
            decodedChar = encodedChar == '+' ? ' ' : encodedChar;  // normal ascii char
        decoded += decodedChar;
    }
    return decoded;
}
