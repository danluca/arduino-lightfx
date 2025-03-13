// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#include "RequestHandlers.h"
#include <string>
#include "mimetable.h"
#include "../WebClient.h"
#include "LogProxy.h"

/**
 * Set of functions targeted at handling a request
 * @param fn general request handler function
 * @param ufn upload/download handler function - given priority in request processing compared with general request handler
 * @param uri request URI pattern to apply this handler to
 * @param method HTTP method to apply this handler to
 */
FunctionRequestHandler::FunctionRequestHandler(THandlerFunction fn, THandlerFunction ufn, const Uri &uri, const HTTPMethod method):
        _fn(std::move(fn)) , _ufn(std::move(ufn)) , _uri(uri.clone()) , _method(method) {
}
FunctionRequestHandler::~FunctionRequestHandler() {
    delete _uri;
}

/**
 * This method checks whether the URI pattern and HTTP method match the handler's locator coordinates.
 * It is currently used by server only in finding routes to remove
 * Identifying which handler to use when processing an HTTP request is accomplished by the \code canHandle(WebClient& client)\endcode
 * @param requestUri request uri to check
 * @param method http method to check
 * @return true if this handler's locator coordinates match; false otherwise
 */
bool FunctionRequestHandler::match(const String &requestUri, const HTTPMethod method) const {
     if (_method != HTTP_ANY && _method != method)
         return false;
     std::vector<String> pathArgs;
     return _uri->canHandle(requestUri, pathArgs);
}

bool FunctionRequestHandler::canHandle(WebClient &client) {
    if (_method != HTTP_ANY && _method != client.request().method())
        return false;
    std::vector<String> pathArgs;
    return _uri->canHandle(client.request().uri(), pathArgs)  && (_filter != nullptr ? _filter(client) : true);
}

bool FunctionRequestHandler::canUpload(WebClient &client) {
    if (!_ufn || !canHandle(client))
        return false;

    return true;
}

bool FunctionRequestHandler::canRaw(WebClient &client) {
    if (!_ufn || _method == HTTP_GET || (_filter != nullptr ? _filter(client) == false : false))
        return false;

    return true;
}

bool FunctionRequestHandler::handle(WebClient &client) {
    _fn(client);
    return true;
}

void FunctionRequestHandler::upload(WebClient &client) {
    _ufn(client);
}

void FunctionRequestHandler::raw(WebClient &client) {
    _ufn(client);
}

FunctionRequestHandler & FunctionRequestHandler::setFilter(const FilterFunction &filter) {
    _filter = filter;
    return *this;
}

/**
 * Initializes the handler of static resources at given base URI from a given local filesystem path. The base URI as well as local filesystem path
 * can be concrete files
 * @param fs filesystem provider
 * @param path base path in the filesystem to read from
 * @param uri base URI allocated to static resources
 * @param cache_header cache header content for response
 */
StaticFileRequestHandler::StaticFileRequestHandler(FS &fs, const char *path, const char *uri, const char *cache_header): _fs(fs), _uri(uri), _path(path),
    _cache_header(cache_header) {
    File f = fs.open(path, "r");
    _isFile = f && f.size() > 0 && (!f.isDirectory());
    f.close();
    log_debug("StaticFileRequestHandler: web uri=%s mapped to physical path=%s, isFile=%d, cache_header=%s", uri, path, _isFile, cache_header ? cache_header : "");
    _baseUriLength = _uri.length();
}

bool StaticFileRequestHandler::match(const String &requestUri, const HTTPMethod method) const {
     if (method != HTTP_GET)
         return false;
     if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri))
         return false;
     return true;
}

bool StaticFileRequestHandler::canHandle(WebClient &client) {
    if (client.request().method() != HTTP_GET)
        return false;
    const String reqUri = client.request().uri();
    if ((_isFile && reqUri != _uri) || !reqUri.startsWith(_uri))
        return false;

    if (_filter != nullptr ? _filter(client) == false : false)
        return false;
    String path;
    getPath(reqUri, path);
    return _fs.exists(path.c_str());
}

bool StaticFileRequestHandler::handle(WebClient &client) {
    //canHandle has been already called to determine whether this method will be invoked; no point in checking again if this handler can handle the request
    const String requestUri = client.request().uri();
    String path;
    getPath(requestUri, path);

    log_debug("StaticFileRequestHandler::handle: request=%s _uri=%s resolved path=%s, isFile=%d", requestUri.c_str(), _uri.c_str(), path.c_str(), _isFile);

    const String contentType = mime::getContentType(path);

    if (_cache_header.length() != 0)
        client.sendHeader(F("Cache-Control"), _cache_header);

    File f = _fs.open(path, "r");
    client.streamFile(f, contentType);
    f.close();
    return true;
}

void StaticFileRequestHandler::getPath(const String &uri, String &path, const char *defaultPath) const {
    path = _path;
    if (_isFile)
        return;
    // Base URI doesn't point to a file. If a directory is requested, look for default file.
    // Append whatever follows this URI in request to get the file path.
    path += uri.substring(_baseUriLength);
    if (path.endsWith("/"))
        path += defaultPath;
}

/**
 * Initializes the handler of static resources at given base URI from a given local filesystem path. The base URI as well as local filesystem path
 * can be concrete files
 * @param fs filesystem provider
 * @param path base path in the filesystem to read from
 * @param uri base URI allocated to static resources
 * @param cache_header cache header content for response
 */
StaticSyncFileRequestHandler::StaticSyncFileRequestHandler(const SynchronizedFS &fs, const char *path, const char *uri, const char *cache_header):
    _fs(fs), _uri(uri), _path(path), _cache_header(cache_header) {
    FileInfo fi{};
    fs.stat(path, &fi);
    _isFile = fi.size >0 && (!fi.isDir);
    log_debug("StaticSyncFileRequestHandler: web uri=%s mapped to physical path=%s, isFile=%d, cache_header=%s", uri, path, _isFile, cache_header ? cache_header : "");
    _baseUriLength = _uri.length();
}

bool StaticSyncFileRequestHandler::match(const String &requestUri, const HTTPMethod method) const {
     if (method != HTTP_GET)
         return false;
     if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri))
         return false;
     return true;
}

bool StaticSyncFileRequestHandler::canHandle(WebClient& client) {
    if (client.request().method() != HTTP_GET)
        return false;
    const String requestUri = client.request().uri();
    if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri))
        return false;

    if (_filter != nullptr ? _filter(client) == false : false)
        return false;
    String path;
    getPath(requestUri, path);
    return _fs.exists(path.c_str());
}

bool StaticSyncFileRequestHandler::handle(WebClient& client) {
    //canHandle has been already called to determine whether this method will be invoked; no point in checking again if this handler can handle the request
    const String requestUri = client.request().uri();
    String path;
    getPath(requestUri, path);

    log_debug("StaticSyncFileRequestHandler::handle: request=%s _uri=%s resolved path=%s, isFile=%d", requestUri.c_str(), _uri.c_str(), path.c_str(), _isFile);

    const String contentType = mime::getContentType(path);

    const auto content = new String();
    _fs.readFile(path.c_str(), content);

    if (_cache_header.length() != 0)
        client.sendHeader(F("Cache-Control"), _cache_header);

    client.streamData(*content, contentType);
    delete content;
    return true;
}

void StaticSyncFileRequestHandler::getPath(const String &uri, String &path, const char *defaultPath) const {
    path = _path;
    if (_isFile)
        return;
    // Base URI doesn't point to a file. If a directory is requested, look for default file.
    // Append whatever follows this URI in request to get the file path.
    path += uri.substring(_baseUriLength);
    if (path.endsWith("/"))
        path += defaultPath;
}

/**
 * Initializes the handler of static resources at given base URI from a given local filesystem path. The base URI as well as local filesystem path
 * can be concrete files
 * @param memRes in memory resources provider
 * @param uri base URI allocated to static resources
 * @param cache_header cache header content for response
 */
StaticInMemoryRequestHandler::StaticInMemoryRequestHandler(const std::map<std::string, const char *> &memRes, const char *uri, const char *cache_header):
    _inMemResources(memRes), _uri(uri), _cache_header(cache_header) {
    log_debug("StaticInMemoryRequestHandler: web uri=%s mapped to in-memory resources, cache_header=%s", uri, cache_header ? cache_header : "");
    _baseUriLength = _uri.length();
}

bool StaticInMemoryRequestHandler::match(const String &requestUri, const HTTPMethod method) const {
     if (method != HTTP_GET) {
         return false;
     }
     String path;
     getPath(requestUri, path);
     path.toLowerCase();
     if (const std::string pathStr = path.c_str(); _inMemResources.find(pathStr) == _inMemResources.end())
         return false;
     return true;
}

bool StaticInMemoryRequestHandler::canHandle(WebClient& client) {
    if (client.request().method() != HTTP_GET)
        return false;
    const String requestUri = client.request().uri();
    String path;
    getPath(requestUri, path);
    path.toLowerCase();
    if (const std::string pathStr = path.c_str(); _inMemResources.find(pathStr) == _inMemResources.end())
        return false;

    if (_filter != nullptr ? _filter(client) == false : false)
        return false;

    return true;
}

bool StaticInMemoryRequestHandler::handle(WebClient& client) {
    //canHandle has been already called to determine whether this method will be invoked; no point in checking again if this handler can handle the request
    const String requestUri = client.request().uri();
    String path;
    getPath(requestUri, path);
    path.toLowerCase();

    log_debug("StaticInMemoryRequestHandler::handle: request=%s _uri=%s resolved path=%s", requestUri.c_str(), _uri.c_str(), path.c_str());

    const String contentType = mime::getContentType(path);

    const auto cacheEntry = _inMemResources.find(path.c_str());
    if (cacheEntry == _inMemResources.end()) {
        log_error("StaticInMemoryRequestHandler::handle: resource not found: %s", path.c_str());
        return false;
    }

    if (_cache_header.length() != 0)
        client.sendHeader(F("Cache-Control"), _cache_header);
    client.streamData(String(cacheEntry->second), contentType);
    return true;
}

void StaticInMemoryRequestHandler::getPath(const String &uri, String &path, const char *defaultPath) const {
    // Enforce all in-memory resource map to have entries named using pattern "/<name.ext>" with a forward slash prefix
    // Append whatever follows this URI in request to get the entry path.
    String basePath = uri.substring(_baseUriLength);
    if (!basePath.startsWith("/"))
        basePath = "/" + basePath;
    if (basePath.endsWith("/"))
        basePath += defaultPath;
    path = basePath;
}
