#pragma once

#include "Uri.h"
#include <fnmatch.h>

class UriGlob : public Uri {

public:
    explicit UriGlob(const char *uri) : Uri(uri) {};
    explicit UriGlob(const String &uri) : Uri(uri) {};

    Uri* clone() const override final {
        return new UriGlob(_uri);
    };

    bool canHandle(const String &requestUri, __attribute__((unused)) std::vector<String> &pathArgs) const override final {
        return fnmatch(_uri.c_str(), requestUri.c_str(), 0) == 0;
    }
};
