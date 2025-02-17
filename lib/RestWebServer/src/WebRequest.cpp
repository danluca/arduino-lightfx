// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "WebRequest.h"

WebRequest::~WebRequest() {
    for (const auto &a: _requestArgs)
        delete a;
    for (const auto &h: _headers)
        delete h;
    _requestArgs.clear();
    _headers.clear();
}

String WebRequest::pathArg(const unsigned int i) const {
    if (i < _pathArgs.size())
        return _pathArgs[i];
    return "";
}

String WebRequest::arg(const String &name) const {
    for (const auto &a: _requestArgs) {
        if (a->key == name) {
            return a->value;
        }
    }
    return "";
}

String WebRequest::arg(const int i) const {
    return i < _requestArgs.size() ? _requestArgs[i]->value : "";
}

String WebRequest::argName(const int i) const {
    return i < _requestArgs.size() ? _requestArgs[i]->key : "";
}

size_t WebRequest::argsCount() const {
    return _requestArgs.size();
}

bool WebRequest::hasArg(const String &name) const {
    for (const auto &a: _requestArgs) {
        if (a->key == name)
            return true;
    }
    return false;
}


String WebRequest::header(const String &name) const {
    for (const auto &h: _headers)
        if (h->key.equalsIgnoreCase(name))
            return h->value;
    return "";
}

String WebRequest::header(const int i) const {
    return i < _headers.size() ? _headers[i]->value : "";
}

String WebRequest::headerName(const int i) const {
    return i < _headers.size() ? _headers[i]->key : "";
}

size_t WebRequest::headersCount() const {
    return _headers.size();
}

bool WebRequest::hasHeader(const String &name) const {
    for (const auto &h: _headers)
        if (h->key.equalsIgnoreCase(name))
            return true;
    return false;
}
