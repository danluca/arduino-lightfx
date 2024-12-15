// Copyright (c) 2024 by Dan Luca. All rights reserved.
//
#include "stringutils.h"

String StringUtils::asString(const CRGB &rgb) {
    //conversion to uint32 uses 0xFF for the alpha channel - we're not interested in the alpha channel
    const uint32_t numClr = rgb.as_uint32_t() & 0xFFFFFF;
    const size_t sz = sprintf(nullptr, "%06X", numClr);
    String str;
    str.reserve(sz);
    sprintf(str.end(), "%06X", numClr);
    return str;
}

String StringUtils::asString(const CRGBSet &rgbSet) {
    size_t sz = sprintf(nullptr, "RGB[%lu]{", rgbSet.len);
    String str;
    str.reserve(rgbSet.len*7 + sz + 1); // 6 digits per color (including space) + prefix + suffix
    for (const CRGB &rgb : rgbSet) {
        sz += sprintf(str.end(), "%06X ", rgb.as_uint32_t() & 0xFFFFFF);
    }
    str.concat("}");
    return str;
}

String StringUtils::asString(const time_t &time) {
    String str;
    str.reserve(20);
    strftime(str.end(), 20, "%Y-%m-%d %H:%M:%S", localtime(&time));
    return str;
}

/**
 *
 * @param rgb
 * @param str
 * @return
 */
size_t StringUtils::toString(const CRGB &rgb, String &str) {
    const String s = asString(rgb);
    str.concat(s);
    return s.length();
}

/**
 *
 * @param rgbSet
 * @param str
 * @return
 */
size_t StringUtils::toString(const CRGBSet &rgbSet, String &str) {
    const String s = asString(rgbSet);
    str.concat(s);
    return s.length();
}

/**
 *
 * @param time
 * @param str
 * @return
 */
size_t StringUtils::toString(const time_t &time, String &str) {
    const String s = asString(time);
    str.concat(s);
    return s.length();
}

size_t prvAppend(String &str, const char *fmt, va_list args) {
    const size_t sz = vsprintf(nullptr, fmt, args);
    str.reserve(str.length() + sz);
    vsprintf(str.end(), fmt, args);
    return sz;
}

size_t StringUtils::append(String &str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const size_t sz = prvAppend(str, fmt, args);
    va_end(args);
    return sz;
}

size_t StringUtils::append(String &str, const __FlashStringHelper *fmt, ...) {
    PGM_P p = reinterpret_cast<PGM_P>(fmt);
    va_list args;
    va_start(args, fmt);
    const size_t sz = prvAppend(str, reinterpret_cast<const char *>(p), args);
    va_end(args);
    return sz;
}
