// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <Arduino.h>
#include <FastLED.h>

inline constexpr auto EMPTY PROGMEM = "";

class StringUtils {
public:
    static size_t toString(const CRGB &rgb, String &str);
    static size_t toString(const CRGBSet &rgbSet, String &str);
    static size_t toString(const time_t &time, String &str);
    static String asString(const CRGB &rgb);
    static String asString(const CRGBSet &rgbSet);
    static String asString(const time_t &time);
    static const char *asString(const bool b) { return b ? "true" : "false";};
    static size_t append(String &str, const char *fmt, ...);
    static size_t append(String &str, const __FlashStringHelper *fmt, ...);
    static String fileName(const String &path);
    static String fileDir(const String &path);
};

#endif //STRINGUTILS_H
