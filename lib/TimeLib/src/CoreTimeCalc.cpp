// MIT License
//
// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "CoreTimeCalc.h"
#include <cinttypes>
#include "TimeDef.h"

// The core time breakdown function that does NOT apply timezone adjustments
void CoreTimeCalc::breakTimeCore(const time_t &timeInput, tmElements_t &tmItems) {
    // Copy of the original breakTime but without timezone adjustments
    time_t time = timeInput;
    tmItems.tm_sec = static_cast<int>(time % 60);
    time /= 60; // now it is minutes
    tmItems.tm_min = static_cast<int>(time % 60);
    time /= 60; // now it is hours
    tmItems.tm_hour = static_cast<int>(time % 24);
    time /= 24; // now it is days
    tmItems.tm_wday = static_cast<int>((time + 4) % 7 + 1);  // Sunday is day 1

    uint16_t year = 0;
    unsigned long days = 0;
    while((days += LEAP_YEAR(year) ? 366 : 365) <= time)
        year++;
    tmItems.tm_year = unixEpochYearToCalendar(year); // the year in timeInput is offset from 1970 since that is what unix time is
    const bool leapYear = LEAP_YEAR(year);
    days -= leapYear ? 366 : 365;
    time -= days; // now it is days in this year, starting at 0
    tmItems.tm_yday = static_cast<int>(time) + 1; // day of the year

    uint8_t month = 0;
    uint8_t monthLength = 0;
    for (month=0; month<12; month++) {
        // month length for February
        if (month==1)
            monthLength = leapYear ? 29 : 28;
        else
            monthLength = monthDays[month];

        if (time >= monthLength)
            time -= monthLength;
        else
            break;
    }
    tmItems.tm_mon = month + 1;  // jan is month 1
    tmItems.tm_mday = static_cast<int>(time) + 1;     // day of the month

    // Initialize timezone fields with defaults (will be set by timezone logic later)
    tmItems.tm_isdst = 0;
    tmItems.tm_offset = 0;
    tmItems.tm_zone = nullptr;
}

// Calculate year directly from time_t without using breakTime
int CoreTimeCalc::calculateYear(const time_t &time) {
    // Direct calculation of year from time_t value
    const time_t days = time / SECS_PER_DAY;  // Convert to days since epoch
    uint16_t year = 0;
    unsigned long accumulatedDays = 0;
    while((accumulatedDays += (LEAP_YEAR(year) ? 366 : 365)) <= days)
        year++;
    return unixEpochYearToCalendar(year);
}

// Implement the core time component extractors
int CoreTimeCalc::hourCore(const time_t t) {
    return static_cast<int>((t % SECS_PER_DAY) / SECS_PER_HOUR);
}

int CoreTimeCalc::minuteCore(const time_t t) {
    return static_cast<int>((t % SECS_PER_HOUR) / SECS_PER_MIN);
}

int CoreTimeCalc::secondCore(const time_t t) {
    return static_cast<int>(t % SECS_PER_MIN);
}

int CoreTimeCalc::dayCore(const time_t t) {
    tmElements_t tm;
    breakTimeCore(t, tm);
    return tm.tm_mday;
}

int CoreTimeCalc::weekdayCore(const time_t t) {
    // Direct calculation without using full breakTime
    return static_cast<int>((t / SECS_PER_DAY + 4) % 7 + 1);  // Sunday is day 1
}

int CoreTimeCalc::monthCore(const time_t t) {
    tmElements_t tm;
    breakTimeCore(t, tm);
    return tm.tm_mon;
}

int CoreTimeCalc::yearCore(const time_t t) {
    return calculateYear(t);
}

int CoreTimeCalc::dayOfYearCore(const time_t t) {
    tmElements_t tm;
    breakTimeCore(t, tm);
    return tm.tm_yday;
}

time_t CoreTimeCalc::makeTimeCore(const tmElements_t &tmItems) {
    // Copy of the original makeTime but without timezone adjustments
    // Note the year element is offset from 1970 if less than 1970, otherwise is absolute year. The time_t represents the year since 1970
    // As always, if you put garbage in, you get garbage out
    // Seconds from 1970 till 1 jan 00:00:00 of the given year
    const int year = tmItems.tm_year > 1970 ? CalendarToUnixEpochYear(tmItems.tm_year) : tmItems.tm_year;
    time_t seconds = year * (SECS_PER_DAY * 365);
    for (int i = 0; i < year; i++) {
        if (LEAP_YEAR(i))
            seconds += SECS_PER_DAY;   // add extra days for leap years
    }

    // add days for this year, months start from 1
    for (int i = 1; i < tmItems.tm_mon; i++) {
        if ((i == 2) && LEAP_YEAR(year))
            seconds += SECS_PER_DAY * 29;
        else
            seconds += SECS_PER_DAY * monthDays[i-1];  //monthDay array starts from 0
    }
    seconds += (tmItems.tm_mday-1) * SECS_PER_DAY;
    seconds += tmItems.tm_hour * SECS_PER_HOUR;
    seconds += tmItems.tm_min * SECS_PER_MIN;
    seconds += tmItems.tm_sec;
    // Note: tm_offset is applied by the timezone layer, not here
    return seconds;
}
