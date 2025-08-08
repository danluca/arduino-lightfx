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
    // Handle negative numbers correctly for modulo operations
    tmItems.tm_sec = (static_cast<int>(time % 60) + 60) % 60;
    time = (time - tmItems.tm_sec) / 60; // now it is minutes
    tmItems.tm_min = (static_cast<int>(time % 60) + 60) % 60;
    time = (time - tmItems.tm_min) / 60; // now it is hours
    tmItems.tm_hour = (static_cast<int>(time % 24) + 24) % 24;
    time = (time - tmItems.tm_hour) / 24; // now it is days
    tmItems.tm_wday = (static_cast<int>((time + 4) % 7) + 7) % 7;  // Sunday is day 0

    int year = UNIX_EPOCH_YEAR;   //start of unix epoch
    long days = 0;
    // Handle both positive and negative time values
    if (time >= 0) {
        // For positive times (after 1/1/1970)
        while((days += daysInYear(year)) <= time)
            year++;
    } else {
        // For negative times (before 1/1/1970)
        year--; // Start from 1969 and go backwards
        while (time < days) {
            days -= daysInYear(year);
            year--;
        }
        year++; // Adjust back since we found the year where time >= days
    }

    tmItems.tm_year = year - TM_EPOCH_YEAR; // year since TM_EPOCH_YEAR (1900)
    const bool leapYear = isLeapYear(year);

    if (time >= 0) {
        days -= leapYear ? 366 : 365;
        time -= days; // now it is days in this year, starting at 0
    } else {
        // For negative times, we need to calculate days within the year differently
        time -= days; // This gives us the days within the current year, which will be zero or negative
        // Adjust to get positive day of year value
        if (time < 0) {
            const int daysInCurrentYear = leapYear ? 366 : 365;
            time += daysInCurrentYear; // Make it positive within the year's range
        }
    }
    tmItems.tm_yday = static_cast<int>(time); // day of the year, starts at 0

    uint8_t month = 0;
    for (month=0; month<12; month++) {
        // month length for February
        if (const uint8_t monthLength = month == 1 ? (leapYear ? 29 : 28) : monthDays[month]; time >= monthLength)
            time -= monthLength;
        else
            break;
    }
    tmItems.tm_mon = month;  // jan is month 0
    tmItems.tm_mday = static_cast<int>(time) + 1;     // day of the month, starts at 1

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
    long accumulatedDays = 0;
    // Handle both positive and negative time values
    if (days >= 0) {
        // For positive times (after 1/1/1970)
        while((accumulatedDays += daysInYear(year)) <= days)
            year++;
    } else {
        // For negative times (before 1/1/1970)
        year--; // Start from 1969 and go backwards
        while (days < accumulatedDays) {
            accumulatedDays -= daysInYear(year);
            year--;
        }
        year++; // Adjust back since we found the year where time >= days
    }
    return year;
}

/**
 * Return the hour of the given time
 * @param t time
 * @return hour 0-23
 */
int CoreTimeCalc::hourCore(const time_t t) {
    return static_cast<int>((t % SECS_PER_DAY) / SECS_PER_HOUR);
}

/**
 * Return the minute of the hour for the given time
 * @param t time
 * @return minute of the hour 0-59
 */
int CoreTimeCalc::minuteCore(const time_t t) {
    return static_cast<int>((t % SECS_PER_HOUR) / SECS_PER_MIN);
}

/**
 * Return the second of the minute for the given time
 * @param t time
 * @return second of the minute 0-59
 */
int CoreTimeCalc::secondCore(const time_t t) {
    return static_cast<int>(t % SECS_PER_MIN);
}

/**
 * Return the day of the month for the given time
 * @param t time
 * @return day of the month 1-31
 */
int CoreTimeCalc::dayCore(const time_t t) {
    tmElements_t tm;
    breakTimeCore(t, tm);
    return tm.tm_mday;
}

/**
 * Return the weekday of the week for the given time
 * @param t time
 * @return day of the week 0-6, Sunday is 0
 */
int CoreTimeCalc::weekdayCore(const time_t t) {
    // Direct calculation without using full breakTime, handling negative values
    return static_cast<int>(((t / SECS_PER_DAY + 4) % 7 + 7) % 7);  // Sunday is day 0
}

/**
 * Return the month of the year for the given time
 * @param t time
 * @return month of the year 0-11, January is 0
 */
int CoreTimeCalc::monthCore(const time_t t) {
    tmElements_t tm{};
    breakTimeCore(t, tm);
    return tm.tm_mon;
}

int CoreTimeCalc::yearCore(const time_t t) {
    return calculateYear(t);
}

int CoreTimeCalc::dayOfYearCore(const time_t t) {
    tmElements_t tm{};
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
    for (int i = 0; i < tmItems.tm_mon; i++) {
        if (i == 1 && leapYear)
            seconds += SECS_PER_DAY * 29;
        else
            seconds += SECS_PER_DAY * monthDays[i];
    }
    seconds += tmItems.tm_mday * SECS_PER_DAY;
    seconds += tmItems.tm_hour * SECS_PER_HOUR;
    seconds += tmItems.tm_min * SECS_PER_MIN;
    seconds += tmItems.tm_sec;
    // Note: tm_offset is applied by the timezone layer, not here
    return seconds;
}
