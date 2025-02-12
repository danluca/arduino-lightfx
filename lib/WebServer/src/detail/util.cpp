// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "util.h"
#include <Arduino.h>
#include <rp2040.h>

String Util::getRandomHexString() {
    char buffer[33];  // buffer to hold 32 Hex Digit + /0
    for (int i = 0; i < 4; i++) {
        sprintf(buffer + (i * 8), "%08lx", rp2040.hwrand32());
    }
    return {buffer};
}

String Util::responseCodeToString(const int code) {
    switch (code) {
        case 100: return F("Continue");
        case 101: return F("Switching Protocols");
        case 200: return F("OK");
        case 201: return F("Created");
        case 202: return F("Accepted");
        case 203: return F("Non-Authoritative Information");
        case 204: return F("No Content");
        case 205: return F("Reset Content");
        case 206: return F("Partial Content");
        case 300: return F("Multiple Choices");
        case 301: return F("Moved Permanently");
        case 302: return F("Found");
        case 303: return F("See Other");
        case 304: return F("Not Modified");
        case 305: return F("Use Proxy");
        case 307: return F("Temporary Redirect");
        case 400: return F("Bad Request");
        case 401: return F("Unauthorized");
        case 402: return F("Payment Required");
        case 403: return F("Forbidden");
        case 404: return F("Not Found");
        case 405: return F("Method Not Allowed");
        case 406: return F("Not Acceptable");
        case 407: return F("Proxy Authentication Required");
        case 408: return F("Request Time-out");
        case 409: return F("Conflict");
        case 410: return F("Gone");
        case 411: return F("Length Required");
        case 412: return F("Precondition Failed");
        case 413: return F("Request Entity Too Large");
        case 414: return F("Request-URI Too Large");
        case 415: return F("Unsupported Media Type");
        case 416: return F("Requested range not satisfiable");
        case 417: return F("Expectation Failed");
        case 500: return F("Internal Server Error");
        case 501: return F("Not Implemented");
        case 502: return F("Bad Gateway");
        case 503: return F("Service Unavailable");
        case 504: return F("Gateway Time-out");
        case 505: return F("HTTP Version not supported");
        default:  return F("");
    }
}

/**
 * Read maximum number of characters specified, engaging a timeout for data to become available from client for each read attempt
 * @param client current WiFi client
 * @param buffer buffer to hold read data
 * @param bufLength length of the buffer (max number of bytes to read)
 * @param timeout_ms amount of time to wait for data to be available per read attempt
 * @return number of bytes/chars read
 */
size_t Util::readBytesWithTimeout(WiFiClient* client, char* buffer, const size_t bufLength, const int timeout_ms) {
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