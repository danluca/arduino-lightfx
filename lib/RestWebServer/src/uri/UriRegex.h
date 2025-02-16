#pragma once

#include "Uri.h"
#include <regex>

class UriRegex : public Uri {

public:
    explicit UriRegex(const char *uri) : Uri(uri) {};
    explicit UriRegex(const String &uri) : Uri(uri) {};

    Uri* clone() const override final {
        return new UriRegex(_uri);
    };

    bool canHandle(const String &requestUri, std::vector<String> &pathArgs) const override final {
        if (Uri::canHandle(requestUri, pathArgs)) {
            return true;
        }
        unsigned int pathArgIndex = 0;
        std::regex rgx(_uri.c_str());
        std::smatch matches;
        std::string s(requestUri.c_str());
        if (std::regex_search(s, matches, rgx)) {
            pathArgs.resize(matches.size() - 1);
            for (size_t i = 1; i < matches.size(); ++i) {  // skip first
                pathArgs[pathArgIndex++] = String(matches[i].str().c_str());
            }
            return true;
        }
        return false;
    }
};
