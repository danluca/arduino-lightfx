// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//
#include "stringutils.h"
#include <FastLED.h>

static inline constexpr auto FS_PATH_SEPARATOR PROGMEM = "/";

String StringUtils::asString(const CRGB &rgb) {
    //conversion to uint32 uses 0xFF for the alpha channel - we're not interested in the alpha channel
    const uint32_t numClr = rgb.as_uint32_t() & 0xFFFFFF;
    char buf[11];   //max hex character for a 32bit number - 2 chars for 0X, 8 hex chars, and null terminator
    // "%06X" produces exactly 6 chars plus null terminator
    const int sz = snprintf(buf, 7, "%06lX", numClr);
    String str(buf, sz);
    return str;
}

String StringUtils::asString(const CRGBSet &rgbSet) {
    String str;
    str.reserve(rgbSet.len*7 + 10 + 1); // 6 digits per color (including space) + prefix + suffix
    // the size of the prefix below is between 7-10 characters (depending on rgbSet size being 1, 2, 3 or 4 digits) - we'll assume 12 chars for the buffer including null terminator (allowing max 11 chars)
    char buf[12]{};
    snprintf(buf, 11, "RGB[%u]{", rgbSet.len);
    str.concat(buf);
    for (const CRGB &rgb : rgbSet) {
        snprintf(buf, 8, "%06lX ", rgb.as_uint32_t() & 0xFFFFFF);
        str.concat(buf);
    }
    str.concat("}");
    return str;
}

/**
 * Useful byte array to hex conversion for small arrays - the returned string is through the stack.
 * @param data byte array
 * @param len size of data
 * @return byte array converted to hex equivalent - a stream of 2 hex digits for each byte in the data
 */
String StringUtils::asHexString(const uint8_t *data, const size_t len) {
    String str;
    str.reserve(len*2);
    char buf[3]{};
    for (size_t i = 0; i < len; i++) {
        snprintf(buf, 3, "%02X", data[i]);
        str.concat(buf, 2);
    }
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

size_t prvAppend(String &str, const char *fmt, va_list args) {
    // Compute the required size without consuming the original va_list
    va_list argsCopy;
    va_copy(argsCopy, args);
    const size_t sz = vsnprintf(nullptr, 0, fmt, argsCopy) + 1; //account for '\0' null terminator not counted by vsnprintf
    va_end(argsCopy);

    // Allocate exact-size temporary buffer on heap to avoid large stack frames
    const auto buf = static_cast<char*>(pvPortMalloc(sz));
    if (!buf) {
        return 0; // allocation failed, nothing appended
    }

    const int written = vsnprintf(buf, sz, fmt, args);
    str.reserve(str.length() + written);
    str.concat(buf);
    vPortFree(buf);
    return written;    // not accounting the null terminator
}

size_t StringUtils::append(String &str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const size_t sz = prvAppend(str, fmt, args);
    va_end(args);
    return sz;
}

size_t StringUtils::append(String &str, const __FlashStringHelper *fmt, ...) {
    const auto p = String(fmt);
    va_list args;
    va_start(args, fmt);
    const size_t sz = prvAppend(str, p.c_str(), args);
    va_end(args);
    return sz;
}

String StringUtils::fileName(const String &path) {
    if (path.endsWith(FS_PATH_SEPARATOR))
        return EMPTY;
    if (const int xSep = path.lastIndexOf(FS_PATH_SEPARATOR); xSep >= 0)
        return path.substring(xSep+1);
    return path;
}

String StringUtils::fileDir(const String &path) {
    const int xSep = path.lastIndexOf(FS_PATH_SEPARATOR);
    if (xSep < 0)
        return EMPTY;
    return path.substring(0, xSep);
}
