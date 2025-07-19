// MIT License
//
// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "CoreTimeCalc.h"
#include <cinttypes>
#include "TimeDef.h"
#include "Timezone.h"

/**
 * Checks whether the year is leap and returns the number of days in that year
 * @param year the (absolute) year to calculate days for (factoring leap or not)
 * @return number of days in the given year - 365 or 366 if leap year
 */
int CoreTimeCalc::daysInYear(const int year) {
    // const bool leapYear = year>0 && !(year%4) && (year%100 || !(year%400));
    return isLeapYear(year) ? 366 : 365;
}

/**
 * The core time breakdown function that does NOT apply timezone adjustments
 * See https://en.cppreference.com/w/c/chrono/tm.html for time elements values/ranges
 * @param timeInput time value (seconds since 1/1/1970) to break
 * @param tmItems time elements structure to populate
 */
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

    int year = UNIX_EPOCH_YEAR;   //start of unix epoch
    unsigned long days = 0;
    //TODO: this doesn't work if time is negative (before 1/1/1970)
    while((days += daysInYear(year)) <= time)
        year++;
    tmItems.tm_year = year - TM_EPOCH_YEAR; // year since TM_EPOCH_YEAR (1900)
    const bool leapYear = isLeapYear(year);
    days -= leapYear ? 366 : 365;
    time -= days; // now it is days in this year, starting at 0
    tmItems.tm_yday = static_cast<int>(time) + 1; // day of the year

    uint8_t month = 0;
    for (month=0; month<12; month++) {
        // month length for February
        if (const uint8_t monthLength = month == 1 ? (leapYear ? 29 : 28) : monthDays[month]; time >= monthLength)
            time -= monthLength;
        else
            break;
    }
    tmItems.tm_mon = month + 1;  // jan is month 1
    tmItems.tm_mday = static_cast<int>(time) + 1;     // day of the month

    // Initialize timezone fields with defaults (will be set by timezone logic later)
    tmItems.tm_isdst = 0;
    tmItems.tm_offset = 0;
    tmItems.tm_zone = utcZone.getName();    //matching the default zero offset
}

// Calculate year directly from time_t without using breakTime
int CoreTimeCalc::calculateYear(const time_t &time) {
    // Direct calculation of year from time_t value
    const time_t days = time / SECS_PER_DAY;  // Convert to days since epoch
    uint16_t year = UNIX_EPOCH_YEAR;
    unsigned long accumulatedDays = 0;
    while((accumulatedDays += daysInYear(year)) <= days)
        year++;
    return year;
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

/**
 * Converts time elements in a \code tmElements\endcode structure into seconds since unix epoch.
 * Note 1: NO VALIDATION is performed on the consistency of time elements values - i.e. the method will convert to
 * seconds a date of 47 February 2015 and a time 75:89:127 (and the resulting time will be normalized to March 21 2015)
 * Note 2: Valid ranges of time elements: tm_mon: 1-12; tm_mday: 1-31; tm_hour: 0-23; tm_min: 0-59; tm_sec: 0-59
 * See https://en.cppreference.com/w/c/chrono/tm.html for expected time elements values/ranges
 * @param tmItems the time elements structure to be converted to time_t (the fields tm_wday, tm_yday, tm_isdst, tm_offset are not used)
 * @return time_t seconds since unix epoch
 */
time_t CoreTimeCalc::makeTimeCore(const tmElements_t &tmItems) {
    // Converting to seconds since unix epoch without timezone adjustments; as always, if you put garbage in, you get garbage out
    // note in the tmElements_t structure, the year is offset since TM_EPOCH_YEAR (1900), and we need to offset since unix epoch 1970
    const int year = tmItems.tm_year + TM_EPOCH_YEAR - UNIX_EPOCH_YEAR;
    time_t seconds = year * (SECS_PER_DAY * 365);
    if (year >= 0)
        for (int i = 0; i < year; i++) {
            if (isLeapYear(i + UNIX_EPOCH_YEAR))
                seconds += SECS_PER_DAY;   // add extra days for leap years
        }
    else
        for (int i = year; i < 0; i++) {
            if (isLeapYear(i + UNIX_EPOCH_YEAR))
                seconds += SECS_PER_DAY;  // add extra days for leap years
        }

    // add days for this year, months start from 1
    const bool leapYear = isLeapYear(year + UNIX_EPOCH_YEAR);
    for (int i = 1; i < tmItems.tm_mon; i++) {
        if (i == 2 && leapYear)
            seconds += SECS_PER_DAY * 29;
        else
            seconds += SECS_PER_DAY * monthDays[i-1];  //monthDay array starts from 0
    }
    seconds += (tmItems.tm_mday-1) * SECS_PER_DAY;  //tm_mday starts from 1
    seconds += tmItems.tm_hour * SECS_PER_HOUR;
    seconds += tmItems.tm_min * SECS_PER_MIN;
    seconds += tmItems.tm_sec;
    // Note: tm_offset is applied by the timezone layer, not here
    return seconds;
}
