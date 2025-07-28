// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <Arduino.h>

struct CRGB;
template<class PIXEL_TYPE> class CPixelView;
typedef CPixelView<CRGB> CRGBSet;

inline constexpr auto EMPTY PROGMEM = "";

class StringUtils {
public:
    static size_t toString(const CRGB &rgb, String &str);
    static size_t toString(const CRGBSet &rgbSet, String &str);
    static String asString(const CRGB &rgb);
    static String asString(const CRGBSet &rgbSet);
    static String asHexString(const uint8_t *data, size_t len);
    static const char *asString(const bool b) { return b ? "true" : "false";};
    static size_t append(String &str, const char *fmt, ...);
    static size_t append(String &str, const __FlashStringHelper *fmt, ...);
    static String fileName(const String &path);
    static String fileDir(const String &path);
};

#endif //STRINGUTILS_H
