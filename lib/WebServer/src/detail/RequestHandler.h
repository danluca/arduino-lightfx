#pragma once

#include <vector>

class RequestHandler {
public:
    virtual ~RequestHandler() = default;

    /* note: old handler API for backward compatibility (const is new) */
    [[nodiscard]] virtual bool canHandle(const HTTPMethod method, const String &uri) {
        (void)method;
        (void)uri;
        return false;
    }
    [[nodiscard]] virtual bool canUpload(const String &uri) {
        (void)uri;
        return false;
    }
    [[nodiscard]] virtual bool canRaw(const String &uri) {
        (void)uri;
        return false;
    }

    /* note: new handler API with support for filters etc. */
    virtual bool canHandle(HTTPServer &server, const HTTPMethod method, const String &uri) {
        (void)server;
        (void)method;
        (void)uri;
        return false;
    }
    virtual bool canUpload(HTTPServer &server, const String &uri) {
        (void)server;
        (void)uri;
        return false;
    }
    virtual bool canRaw(HTTPServer &server, const String &uri) {
        (void)server;
        (void)uri;
        return false;
    }
    virtual bool handle(HTTPServer& server, const HTTPMethod requestMethod, const String &requestUri) {
        (void) server;
        (void) requestMethod;
        (void) requestUri;
        return false;
    }
    virtual void upload(HTTPServer& server, const String &requestUri, const HTTPUpload& upload) {
        (void) server;
        (void) requestUri;
        (void) upload;
    }
    virtual void raw(HTTPServer& server, const String &requestUri, HTTPRaw& raw) {
        (void) server;
        (void) requestUri;
        (void) raw;
    }

    [[nodiscard]] RequestHandler* next() const {
        return _next;
    }
    void next(RequestHandler* r) {
        _next = r;
    }

    virtual RequestHandler& setFilter(const std::function<bool(HTTPServer&)> &filter) {
        (void)filter;
        return *this;
    }

private:
    RequestHandler* _next = nullptr;

protected:
    std::vector<String> pathArgs{};

public:
    const String& pathArg(const unsigned int i) {
        assert(i < pathArgs.size());
        return pathArgs[i];
    }
};
