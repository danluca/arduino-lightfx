// MIT License
//
// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef CORE_TIME_CALC_H
#define CORE_TIME_CALC_H

#include "Arduino.h"

// Forward declarations
typedef tm tmElements_t;
extern const uint8_t monthDays[];

class CoreTimeCalc {
public:
    // Core time breakdown without timezone adjustment
    static void breakTimeCore(const time_t &timeInput, tmElements_t &tmItems);

    // Core time component extractors that don't rely on timezone
    static int hourCore(time_t t);
    static int minuteCore(time_t t);
    static int secondCore(time_t t);
    static int dayCore(time_t t);
    static int weekdayCore(time_t t);
    static int monthCore(time_t t);
    static int yearCore(time_t t);
    static int dayOfYearCore(time_t t);

    // Make time without timezone adjustments
    static time_t makeTimeCore(const tmElements_t &tmItems);

    // Calculate year directly from time_t without using breakTime
    static int calculateYear(const time_t &time);
};

#endif // CORE_TIME_CALC_H
