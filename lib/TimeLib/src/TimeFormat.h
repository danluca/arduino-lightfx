// MIT License
//
// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef TIMEFORMAT_H
#define TIMEFORMAT_H
#include <Arduino.h>

class TimeFormat {
public:
    static size_t toString(const time_t &time, String &str);
    static size_t toString(const time_t &time, const char* formatter, String &str);
    static String asString(const time_t &time);
    static String asStringMs(const time_t &timeMs);
    static String asString(const time_t &time, const char* formatter);
    /* date strings */
    static size_t monthStr(uint8_t month, char* buffer);
    static size_t dayStr(uint8_t day, char* buffer);
    static size_t monthShortStr(uint8_t month, char* buffer);
    static size_t dayShortStr(uint8_t day, char* buffer);
};

#endif //TIMEFORMAT_H
