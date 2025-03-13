// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#ifndef WEBREQUEST_H
#define WEBREQUEST_H

#include <Arduino.h>
#include "WiFiNINA.h"
#include "HTTP_Method.h"

class WebClient;
inline constexpr char AUTHORIZATION_HEADER[] PROGMEM = "Authorization";
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
enum HTTPRawStatus { RAW_START, RAW_WRITE, RAW_END, RAW_ABORTED };

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 1436
#endif

#ifndef HTTP_RAW_BUFLEN
#define HTTP_RAW_BUFLEN 1436
#endif

#ifndef HTTP_MAX_POST_DATA_LENGTH
#define HTTP_MAX_POST_DATA_LENGTH 4096
#endif

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

typedef struct {
    String key;
    String value;
} NameValuePair;

class WebRequest {
  public:
    WebRequest() = default;
    virtual ~WebRequest();

    [[nodiscard]] String uri() const { return _reqUri; }
    [[nodiscard]] String url() const { return _reqUrl; }
    [[nodiscard]] HTTPMethod method() const { return _method; }

    [[nodiscard]] String pathArg(unsigned int i) const; // get request path argument by number
    [[nodiscard]] String arg(const String& name) const;        // get request argument value by name
    [[nodiscard]] String arg(int i) const;              // get request argument value by number
    [[nodiscard]] String argName(int i) const;          // get request argument name by number
    [[nodiscard]] size_t argsCount() const;                     // get arguments count
    [[nodiscard]] bool hasArg(const String& name) const;       // check if argument exists
    [[nodiscard]] String header(const String& name) const;     // get request header value by name
    [[nodiscard]] String header(int i) const;           // get request header value by number
    [[nodiscard]] String headerName(int i) const;       // get request header name by number
    [[nodiscard]] size_t headersCount() const;                  // get header count
    [[nodiscard]] bool hasHeader(const String& name) const;    // check if header exists
    [[nodiscard]] const String& body() { return _requestBody; }
    [[nodiscard]] const String& boundary() { return _boundaryStr; }
    [[nodiscard]] size_t contentLength() const { return _contentLength; }    // return "content-length" of incoming HTTP header from "_currentClient"
    [[nodiscard]] String httpVersion() const { return _httpVersion; }
    [[nodiscard]] long httpVersionNumeric() const {
      String version = _httpVersion;
      version.replace(".", "");
      return version.toInt();
    }

protected:
    HTTPMethod  _method{};
    String      _reqUrl;
    String      _reqUri;
    String      _httpVersion;
    std::deque<NameValuePair*> _headers{};
    size_t           _contentLength{};	// "Content-Length" from header of incoming POST or GET request
    String           _requestBody;
    String           _boundaryStr;   //we're not handling multipart/form-data, but keeping it in case we need to start parsing forms
    std::vector<String> _pathArgs{};
    std::deque<NameValuePair*> _requestArgs{};
    // String           _sNonce;  // Store nonce and opaque for future comparison
    // String           _sOpaque;
    // String           _sRealm;  // Store the Auth realm between Calls

    friend class WebClient;
};

#endif //WEBREQUEST_H
