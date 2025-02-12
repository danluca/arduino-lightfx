#pragma once

#include <Arduino.h>
#include <vector>

class Uri {
protected:
    const String _uri;

public:
    Uri(const char *uri) : _uri(uri) {}
    Uri(const String &uri) : _uri(uri) {}
    Uri(const __FlashStringHelper *uri) : _uri(String(uri)) {}
    virtual ~Uri() = default;

    [[nodiscard]] String uri() const { return _uri;}

    [[nodiscard]] virtual Uri* clone() const {
        return new Uri(_uri);
    };

    [[nodiscard]] virtual bool canHandle(const String &requestUri, __attribute__((unused)) std::vector<String> &pathArgs) const {
        return _uri == requestUri;
    }

    static String urlDecode(const String& text) {
        String decoded = "";
        decoded.reserve(text.length());
        char temp[] = "0x00";
        const unsigned int len = text.length();
        unsigned int i = 0;
        while (i < len) {
            char decodedChar;
            if (const char encodedChar = text.charAt(i++); (encodedChar == '%') && (i + 1 < len)) {
                temp[2] = text.charAt(i++);
                temp[3] = text.charAt(i++);

                decodedChar = strtol(temp, nullptr, 16);
            } else
                decodedChar = encodedChar == '+' ? ' ' : encodedChar;  // normal ascii char
            decoded += decodedChar;
        }
        return decoded;
    }
};
