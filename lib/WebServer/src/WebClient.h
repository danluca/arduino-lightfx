// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#ifndef WEBCLIENT_H
#define WEBCLIENT_H

#include <Arduino.h>
#include "../../RP2040WiFiNina/src/WiFiNINA.h"
#include <functional>
#include <memory>
#include "WebRequest.h"
#include "detail/StringStream.h"
#include "detail/RequestHandlers.h"

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

enum HTTPClientStatus { HC_WAIT_READ, HC_WAIT_CLOSE, HC_COMPLETED };
enum HTTPAuthMethod { BASIC_AUTH, DIGEST_AUTH };

class HTTPServer;

class WebClient {
  public:
    WebClient(HTTPServer* server, const WiFiClient& client);
    virtual ~WebClient();

    void close();
    void requestAuthentication(HTTPAuthMethod mode = BASIC_AUTH, const char* realm = nullptr, const String& authFailMsg = String(""));

    enum ClientAction { CLIENT_REQUEST_CAN_CONTINUE, CLIENT_REQUEST_IS_HANDLED, CLIENT_MUST_STOP, CLIENT_IS_GIVEN };

    WiFiClient& rawClient() { return _rawWifiClient; }
    [[nodiscard]] HTTPUpload& upload() const { return *_uploadBody; }
    [[nodiscard]] HTTPRaw& raw() const { return *_rawBody; }
    [[nodiscard]] WebRequest& request() const { return *_request; }
    HTTPClientStatus handleRequest();

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

    void setContentLength(size_t contentLength);
    void sendHeader(const String& name, const String& value, bool first = false);
    size_t sendContent(const String &content);
    size_t sendContent(const char *content, size_t contentLength);
    size_t sendContent_P(PGM_P content);
    size_t sendContent_P(PGM_P content, size_t size);
    bool chunkedResponseModeStart_P(const int code, PGM_P content_type) {
        if (_request->httpVersionNumeric() < 11) {
            // no chunk mode in HTTP/1.0
            return false;
        }
        setContentLength(CONTENT_LENGTH_UNKNOWN);
        send(code, content_type, "");
        return true;
    }
    bool chunkedResponseModeStart(const int code, const char* content_type) { return chunkedResponseModeStart_P(code, content_type); }
    bool chunkedResponseModeStart(const int code, const String& content_type) { return chunkedResponseModeStart_P(code, content_type.c_str()); }
    void chunkedResponseFinalize() { sendContent(""); }
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
    virtual size_t _currentClientWrite(Stream& s) { return _rawWifiClient.write(s); }
    void _finalizeResponse();
    ClientAction _handleRawData();
    ClientAction _parseRequest();
    void _processRequest();
    void _parseHttpHeaders();
    void _parseArguments(const String& data) const;
    void _uploadWriteByte(uint8_t b);
    void _uploadWriteBytes(const uint8_t* b, size_t len);
    int _uploadReadByte();
    size_t _uploadReadBytes(uint8_t* buf, size_t len);
    void _prepareHeader(String& response, int code, const char* content_type, size_t contentLength);
    size_t _streamFileCore(size_t fileSize, const String &fileName, const String &contentType, int code = 200);

    HTTPServer* _server;
    WiFiClient  _rawWifiClient;
    HTTPClientStatus _status;
    time_t      _startHandlingTime;
    RequestHandler*  _requestHandler;   //currently matched request handler; the other handlers (not found, file upload/download) are global per server
    std::unique_ptr<HTTPUpload> _uploadBody{};
    std::unique_ptr<HTTPRaw>    _rawBody{};

    WebRequest* _request;

    // http response elements
    size_t           _contentLength;
    String           _responseHeaders;
    bool             _chunked;

};



#endif //WEBCLIENT_H
