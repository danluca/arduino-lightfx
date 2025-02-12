#pragma once
#include <functional>
#include <map>
#include "../HTTP_Method.h"
#include "../Uri.h"
#include "../../../FilesystemTask/src/filesystem.h"

class WebClient;
typedef std::function<void(WebClient& webClient)> THandlerFunction;
typedef std::function<bool(WebClient& webClient)> FilterFunction;

class RequestHandler {
public:
    virtual ~RequestHandler() = default;

    [[nodiscard]] virtual bool match(const String &requestUri, const HTTPMethod method) const { return false; }
    virtual bool canHandle(WebClient& client) { return false; };
    virtual bool canUpload(WebClient& client) { return false; }
    virtual bool canRaw(WebClient& client) { return false; }
    virtual bool handle(WebClient& client) { return false; }
    virtual void upload(WebClient& client) { }
    virtual void raw(WebClient& client) { }
    virtual RequestHandler& setFilter(const FilterFunction &filter) { return *this; }
};

class FunctionRequestHandler : public RequestHandler {
public:
    FunctionRequestHandler(THandlerFunction fn, THandlerFunction ufn, const Uri &uri, HTTPMethod method);

    ~FunctionRequestHandler() override;

    [[nodiscard]] bool match(const String &requestUri, HTTPMethod method) const override;

    [[nodiscard]] bool canHandle(WebClient& client) override;

    [[nodiscard]] bool canUpload(WebClient& client) override;

    [[nodiscard]] bool canRaw(WebClient& client) override;

    bool handle(WebClient& client) override;

    void upload(WebClient& client) override;

    void raw(WebClient& client) override;

    FunctionRequestHandler& setFilter(const FilterFunction &filter) override;

protected:
    THandlerFunction _fn;
    THandlerFunction _ufn;
    FilterFunction _filter; // _filter should return 'true' when the request should be handled and 'false' when the request should be ignored
    const Uri *_uri;
    const HTTPMethod _method;
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
    StaticFileRequestHandler(FS& fs, const char* path, const char* uri, const char* cache_header);

    [[nodiscard]] bool match(const String &requestUri, HTTPMethod method) const override;
    bool canHandle(WebClient& client) override;
    bool handle(WebClient& client) override;
    void getPath(const String& uri, String& path, const char* defaultPath = "index.html") const;

    /**
     *
     * @param path path to examine for determining content type
     * @return the corresponding content type to the file extension of the path, falls back to application/octet-stream if none match
     */
    static String getContentType(const String& path);

    StaticFileRequestHandler& setFilter(const FilterFunction &filter) override {
        _filter = filter;
        return *this;
    }

protected:
    FilterFunction _filter; // _filter should return 'true' when the request should be handled and 'false' when the request should be ignored
    FS _fs;
    String _uri;
    String _path;
    String _cache_header;
    bool _isFile;
    size_t _baseUriLength;
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
    StaticSyncFileRequestHandler(const SynchronizedFS& fs, const char* path, const char* uri, const char* cache_header);

    [[nodiscard]] bool match(const String &requestUri, HTTPMethod method) const override;
    bool canHandle(WebClient& client) override;
    bool handle(WebClient& client) override;
    void getPath(const String& uri, String& path, const char* defaultPath = "index.html") const;

    /**
     *
     * @param path path to examine for determining content type
     * @return the corresponding content type to the file extension of the path, falls back to application/octet-stream if none match
     */
    static String getContentType(const String& path);

    StaticSyncFileRequestHandler& setFilter(const FilterFunction &filter) override {
        _filter = filter;
        return *this;
    }

protected:
    FilterFunction _filter; // _filter should return 'true' when the request should be handled and 'false' when the request should be ignored
    SynchronizedFS _fs;
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
    StaticInMemoryRequestHandler(const std::map<std::string, const char*> &memRes, const char* uri, const char* cache_header);

    [[nodiscard]] bool match(const String &requestUri, HTTPMethod method) const override;
    bool canHandle(WebClient& client) override;
    bool handle(WebClient& client) override;
    void getPath(const String& uri, String& path, const char* defaultPath = "index.html") const;

    StaticInMemoryRequestHandler& setFilter(const FilterFunction &filter) override {
        _filter = filter;
        return *this;
    }

protected:
    FilterFunction _filter; // _filter should return 'true' when the request should be handled and 'false' when the request should be ignored
    const std::map<std::string, const char*> &_inMemResources;
    String _uri;
    String _cache_header;
    size_t _baseUriLength;
};
