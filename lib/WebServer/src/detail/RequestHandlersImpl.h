#pragma once

#include "RequestHandler.h"
#include "mimetable.h"
#include <api/String.h>
#include <map>
#include <string>
#include <utility>
#include "Uri.h"
#include "LogProxy.h"
#include "../../../FilesystemTask/src/filesystem.h"

using namespace mime;

class FunctionRequestHandler : public RequestHandler {
public:
    FunctionRequestHandler(HTTPServer::THandlerFunction fn, HTTPServer::THandlerFunction ufn, const Uri &uri, const HTTPMethod method)
        : _fn(std::move(fn)) , _ufn(std::move(ufn)) , _uri(uri.clone()) , _method(method) {
        _uri->initPathArgs(pathArgs);
    }

    ~FunctionRequestHandler() override {
        delete _uri;
    }

    [[nodiscard]] bool canHandle(const HTTPMethod requestMethod, const String &requestUri) override  {
        if (_method != HTTP_ANY && _method != requestMethod) {
            return false;
        }

        return _uri->canHandle(requestUri, pathArgs);
    }

    [[nodiscard]] bool canUpload(const String &requestUri) override  {
        if (!_ufn || !canHandle(HTTP_POST, requestUri)) {
            return false;
        }

        return true;
    }
    [[nodiscard]] bool canRaw(const String &requestUri) override {
        (void) requestUri;
        if (!_ufn || _method == HTTP_GET) {
            return false;
        }

        return true;
    }

    bool canHandle(HTTPServer &server, const HTTPMethod requestMethod, const String &requestUri) override {
        if (_method != HTTP_ANY && _method != requestMethod) {
            return false;
        }

        return _uri->canHandle(requestUri, pathArgs) && (_filter != nullptr ? _filter(server) : true);
    }

    bool canUpload(HTTPServer &server, const String &requestUri) override {
        if (!_ufn || !canHandle(server, HTTP_POST, requestUri)) {
            return false;
        }

        return true;
    }
    bool canRaw(HTTPServer &server, const String &requestUri) override {
        (void) requestUri;
        if (!_ufn || _method == HTTP_GET || (_filter != nullptr ? _filter(server) == false : false)) {
            return false;
        }

        return true;
    }

    bool handle(HTTPServer& server, const HTTPMethod requestMethod, const String &requestUri) override {
        if (!canHandle(server, requestMethod, requestUri)) {
            return false;
        }

        _fn();
        return true;
    }

    void upload(HTTPServer& server, const String &requestUri, const HTTPUpload& upload) override {
        (void) upload;
        if (canUpload(server, requestUri)) {
            _ufn();
        }
    }

    void raw(HTTPServer& server, const String &requestUri, HTTPRaw& raw) override {
        (void)server;
        (void)raw;
        if (canRaw(server, requestUri)) {
            _ufn();
        }
    }

    FunctionRequestHandler& setFilter(const HTTPServer::FilterFunction &filter) override {
        _filter = filter;
        return *this;
    }

protected:
    HTTPServer::THandlerFunction _fn;
    HTTPServer::THandlerFunction _ufn;
    // _filter should return 'true' when the request should be handled and 'false' when the request should be ignored
    HTTPServer::FilterFunction _filter;
    Uri *_uri;
    HTTPMethod _method;
};

/**
 * @class StaticSyncFileRequestHandler
 * @brief A request handler for serving static files and directories using the file system.
 *
 * StaticSyncFileRequestHandler is derived from the RequestHandler base class and facilitates
 * handling HTTP GET requests by serving static content from the file system. It supports
 * both file and directory requests, and can optionally filter requests based on user-defined
 * criteria or provide caching headers.
 * NOTE: This class makes use of the SynchronizedFS to access the LittleFS filesystem through a dedicated thread/task.
 */
class StaticSyncFileRequestHandler : public RequestHandler {
public:
    /**
     * Initializes the handler of static resources at given base URI from a given local filesystem path. The base URI as well as local filesystem path
     * can be concrete files
     * @param fs filesystem provider
     * @param memRes in memory resources provider
     * @param path base path in the filesystem to read from
     * @param uri base URI allocated to static resources
     * @param cache_header cache header content for response
     */
    StaticSyncFileRequestHandler(const SynchronizedFS& fs, const char* path, const char* uri, const char* cache_header)
        : _fs(fs), _uri(uri), _path(path), _cache_header(cache_header) {
        const FileInfo fi = fs.info(path);
        _isFile = fi.size >0 && (!fi.isDir);
        log_debug("StaticFileRequestHandler: web uri=%s mapped to physical path=%s, isFile=%d, cache_header=%s", uri, path, _isFile, cache_header ? cache_header : "");
        _baseUriLength = _uri.length();
    }

    bool canHandle(const HTTPMethod requestMethod, const String &requestUri) override  {
        if (requestMethod != HTTP_GET) {
            return false;
        }

        if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri)) {
            return false;
        }

        String path;
        getPath(requestUri, path);
        return _fs.exists(path.c_str());
    }

    bool canHandle(HTTPServer &server, const HTTPMethod requestMethod, const String &requestUri) override {
        if (requestMethod != HTTP_GET) {
            return false;
        }

        if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri)) {
            return false;
        }

        if (_filter != nullptr ? _filter(server) == false : false) {
            return false;
        }
        String path;
        getPath(requestUri, path);
        return _fs.exists(path.c_str());
    }

    bool handle(HTTPServer& server, const HTTPMethod requestMethod, const String &requestUri) override {
        //canHandle has been already called to determine whether this method will be invoked; no point in checking again if this handler can handle the request
        String path;
        getPath(requestUri, path);

        log_debug("StaticFileRequestHandler::handle: request=%s _uri=%s resolved path=%s, isFile=%d", requestUri.c_str(), _uri.c_str(), path.c_str(), _isFile);

        const String contentType = getContentType(path);
        bool pathExists = _fs.exists(path.c_str());

        // look for gz file, only if the original specified path is not a gz.  So part only works to send gzip via content encoding when a non-compressed is asked for
        // if you point the path to gzip you will serve the gzip as content type "application/x-gzip", not text or javascript etc...
        if (!path.endsWith(FPSTR(mimeTable[gz].endsWith)) && !pathExists)  {
            if (const String pathWithGz = path + FPSTR(mimeTable[gz].endsWith); _fs.exists(pathWithGz.c_str())) {
                path += FPSTR(mimeTable[gz].endsWith);
                pathExists = true;
            }
        }

        if (!pathExists) {
            log_error("StaticFileRequestHandler::handle: file not found: %s", path.c_str());
            return false;
        }
        const auto content = new String();
        _fs.readFile(path.c_str(), content);

        if (_cache_header.length() != 0) {
            server.sendHeader(F("Cache-Control"), _cache_header);
        }

        server.streamData(*content, contentType);
        delete content;
        return true;
    }

    void getPath(const String& uri, String& path, const char* defaultPath = "index.html") const {
        path = _path;
        if (_isFile)
            return;
        // Base URI doesn't point to a file. If a directory is requested, look for default file.
        // Append whatever follows this URI in request to get the file path.
        path += uri.substring(_baseUriLength);
        if (path.endsWith("/")) {
            path += defaultPath;
        }
    }

    /**
     *
     * @param path path to examine for determining content type
     * @return the corresponding content type to the file extension of the path, falls back to application/octet-stream if none match
     */
    static String getContentType(const String& path) {
        char buff[sizeof(mimeTable[0].mimeType)];
        // Check all entries but last one for match, return if found
        constexpr size_t mimeSize = std::size(mimeTable);
        for (size_t i = 0; i < mimeSize - 1; i++) {
            strcpy_P(buff, mimeTable[i].endsWith);
            if (path.endsWith(buff)) {
                strcpy_P(buff, mimeTable[i].mimeType);
                return {buff};
            }
        }
        // Fall-through and just return default type
        strcpy_P(buff, mimeTable[mimeSize - 1].mimeType);
        return {buff};
    }

    StaticSyncFileRequestHandler& setFilter(const HTTPServer::FilterFunction &filter) override {
        _filter = filter;
        return *this;
    }

protected:
    // _filter should return 'true' when the request should be handled
    // and 'false' when the request should be ignored
    HTTPServer::FilterFunction _filter{};
    SynchronizedFS _fs;
    String _uri;
    String _path;
    String _cache_header;
    bool _isFile;
    size_t _baseUriLength;
};

/**
 * @class StaticFileRequestHandler
 * @brief A request handler for serving static files and directories using the file system.
 *
 * StaticFileRequestHandler is derived from the RequestHandler base class and facilitates
 * handling HTTP GET requests by serving static content from the file system. It supports
 * both file and directory requests, and can optionally filter requests based on user-defined
 * criteria or provide caching headers.
 * NOTE: This class makes direct access to FileSystem object provided (likely LittleFS concrete type) on a different thread/task
 * than the filesystem one (LittleFS is known not to be thread safe). If things get hairy, odd hanging, etc. may want to revisit how this
 * class retrieves and streams the content of a file from a different task than the dedicated filesystem one.
 */
class StaticFileRequestHandler : public RequestHandler {
public:
    /**
     * Initializes the handler of static resources at given base URI from a given local filesystem path. The base URI as well as local filesystem path
     * can be concrete files
     * @param fs filesystem provider
     * @param memRes in memory resources provider
     * @param path base path in the filesystem to read from
     * @param uri base URI allocated to static resources
     * @param cache_header cache header content for response
     */
    StaticFileRequestHandler(FS& fs, const char* path, const char* uri, const char* cache_header)
        : _fs(fs), _uri(uri), _path(path), _cache_header(cache_header) {
        const File f = fs.open(path, "r");
        _isFile = f && f.size() > 0 && (!f.isDirectory());
        log_debug("StaticFileRequestHandler: web uri=%s mapped to physical path=%s, isFile=%d, cache_header=%s", uri, path, _isFile, cache_header ? cache_header : "");
        _baseUriLength = _uri.length();
    }

    bool canHandle(const HTTPMethod requestMethod, const String &requestUri) override  {
        if (requestMethod != HTTP_GET) {
            return false;
        }

        if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri)) {
            return false;
        }

        String path;
        getPath(requestUri, path);
        return _fs.exists(path.c_str());
    }

    bool canHandle(HTTPServer &server, const HTTPMethod requestMethod, const String &requestUri) override {
        if (requestMethod != HTTP_GET) {
            return false;
        }

        if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri)) {
            return false;
        }

        if (_filter != nullptr ? _filter(server) == false : false) {
            return false;
        }
        String path;
        getPath(requestUri, path);
        return _fs.exists(path.c_str());
    }

    bool handle(HTTPServer& server, const HTTPMethod requestMethod, const String &requestUri) override {
        //canHandle has been already called to determine whether this method will be invoked; no point in checking again if this handler can handle the request
        String path;
        getPath(requestUri, path);

        log_debug("StaticFileRequestHandler::handle: request=%s _uri=%s resolved path=%s, isFile=%d", requestUri.c_str(), _uri.c_str(), path.c_str(), _isFile);

        const String contentType = getContentType(path);
        bool pathExists = _fs.exists(path.c_str());

        // look for gz file, only if the original specified path is not a gz.  So part only works to send gzip via content encoding when a non-compressed is asked for
        // if you point the path to gzip you will serve the gzip as content type "application/x-gzip", not text or javascript etc...
        if (!path.endsWith(FPSTR(mimeTable[gz].endsWith)) && !pathExists)  {
            if (const String pathWithGz = path + FPSTR(mimeTable[gz].endsWith); _fs.exists(pathWithGz.c_str())) {
                path += FPSTR(mimeTable[gz].endsWith);
                pathExists = true;
            }
        }

        if (!pathExists) {
            log_error("StaticFileRequestHandler::handle: file not found: %s", path.c_str());
            return false;
        }

        if (_cache_header.length() != 0) {
            server.sendHeader(F("Cache-Control"), _cache_header);
        }

        File f = _fs.open(path, "r");
        server.streamFile(f, contentType);
        f.close();
        return true;
    }

    void getPath(const String& uri, String& path, const char* defaultPath = "index.html") const {
        path = _path;
        if (_isFile)
            return;
        // Base URI doesn't point to a file. If a directory is requested, look for default file.
        // Append whatever follows this URI in request to get the file path.
        path += uri.substring(_baseUriLength);
        if (path.endsWith("/")) {
            path += defaultPath;
        }
    }

    /**
     *
     * @param path path to examine for determining content type
     * @return the corresponding content type to the file extension of the path, falls back to application/octet-stream if none match
     */
    static String getContentType(const String& path) {
        char buff[sizeof(mimeTable[0].mimeType)];
        // Check all entries but last one for match, return if found
        constexpr size_t mimeSize = std::size(mimeTable);
        for (size_t i = 0; i < mimeSize - 1; i++) {
            strcpy_P(buff, mimeTable[i].endsWith);
            if (path.endsWith(buff)) {
                strcpy_P(buff, mimeTable[i].mimeType);
                return {buff};
            }
        }
        // Fall-through and just return default type
        strcpy_P(buff, mimeTable[mimeSize - 1].mimeType);
        return {buff};
    }

    StaticFileRequestHandler& setFilter(const HTTPServer::FilterFunction &filter) override {
        _filter = filter;
        return *this;
    }

protected:
    // _filter should return 'true' when the request should be handled
    // and 'false' when the request should be ignored
    HTTPServer::FilterFunction _filter{};
    FS _fs;
    String _uri;
    String _path;
    String _cache_header;
    bool _isFile;
    size_t _baseUriLength;
};

/**
 * @class StaticInMemoryRequestHandler
 * @brief A request handler for serving static resources stored in memory.
 *
 * StaticInMemoryRequestHandler is a specialized RequestHandler that serves static
 * resources directly from memory. It supports handling HTTP GET requests for resources
 * mapped to a base URI, with customizable caching headers and optional filtering logic.
 *
 * This handler ensures efficient retrieval of in-memory resources and is suitable for
 * environments where file system I/O needs to be minimized or avoided, such as embedded
 * systems or high-performance applications.
 */
class StaticInMemoryRequestHandler : public RequestHandler {
public:
    /**
     * Initializes the handler of static resources at given base URI from a given local filesystem path. The base URI as well as local filesystem path
     * can be concrete files
     * @param fs filesystem provider
     * @param memRes in memory resources provider
     * @param path base path in the filesystem to read from
     * @param uri base URI allocated to static resources
     * @param cache_header cache header content for response
     */
    StaticInMemoryRequestHandler(const std::map<std::string, const char*> &memRes, const char* uri, const char* cache_header)
        : _inMemResources(memRes), _uri(uri), _cache_header(cache_header) {
        log_debug("StaticInMemoryRequestHandler: web uri=%s mapped to in-memory resources, cache_header=%s", uri, cache_header ? cache_header : ""); // issue 5506 - cache_header can be nullptr
        _baseUriLength = _uri.length();
    }

    [[nodiscard]] bool canHandle(const HTTPMethod requestMethod, const String &requestUri) override  {
        if (requestMethod != HTTP_GET) {
            return false;
        }

        String path;
        getPath(requestUri, path);
        path.toLowerCase();
        if (const std::string pathStr = path.c_str(); _inMemResources.find(pathStr) == _inMemResources.end()) {
            return false;
        }

        return true;
    }

    bool canHandle(HTTPServer &server, const HTTPMethod requestMethod, const String &requestUri) override {
        if (!canHandle(requestMethod, requestUri))
            return false;

        if (_filter != nullptr ? _filter(server) == false : false) {
            return false;
        }

        return true;
    }

    bool handle(HTTPServer& server, const HTTPMethod requestMethod, const String &requestUri) override {
        //canHandle has been already called to determine whether this method will be invoked; no point in checking again if this handler can handle the request
        String path;
        getPath(requestUri, path);
        path.toLowerCase();

        log_debug("StaticInMemoryRequestHandler::handle: request=%s _uri=%s resolved path=%s", requestUri.c_str(), _uri.c_str(), path.c_str());

        const String contentType = getContentType(path);

        const auto cacheEntry = _inMemResources.find(path.c_str());
        if (cacheEntry == _inMemResources.end()) {
            log_error("StaticInMemoryRequestHandler::handle: resource not found: %s", path.c_str());
            return false;
        }

        if (_cache_header.length() != 0) {
            server.sendHeader(F("Cache-Control"), _cache_header);
        }
        server.streamData(String(cacheEntry->second), contentType);
        return true;
    }

    void getPath(const String& uri, String& path, const char* defaultPath = "index.html") const {
        // Enforce all in-memory resource map to have entries named using pattern "/<name.ext>" with a forward slash prefix
        // Append whatever follows this URI in request to get the entry path.
        String basePath = uri.substring(_baseUriLength);
        if (!basePath.startsWith("/"))
            basePath = "/" + basePath;
        if (basePath.endsWith("/"))
            basePath += defaultPath;
        path = basePath;
    }

    StaticInMemoryRequestHandler& setFilter(const HTTPServer::FilterFunction &filter) override {
        _filter = filter;
        return *this;
    }

protected:
    // _filter should return 'true' when the request should be handled
    // and 'false' when the request should be ignored
    HTTPServer::FilterFunction _filter{};
    const std::map<std::string, const char*> &_inMemResources;
    String _uri;
    String _cache_header;
    size_t _baseUriLength;
};
