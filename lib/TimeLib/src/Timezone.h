/*----------------------------------------------------------------------*
 * Arduino Timezone Library                                             *
 * Jack Christensen Mar 2012                                            *
 *                                                                      *
 * Arduino Timezone Library Copyright (C) 2018,2025 by Jack Christensen and  *
 * licensed under GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html   *
 *----------------------------------------------------------------------*/
#pragma once
#ifndef TIMEZONE_H_INCLUDED
#define TIMEZONE_H_INCLUDED
#include <CoreTimeCalc.h>
#include "TimeDef.h"

// convenient constants for TimeChangeRules
enum week_t:uint8_t {Last, First, Second, Third, Fourth};
enum dow_t:uint8_t {Sun=0, Mon, Tue, Wed, Thu, Fri, Sat};
enum month_t:uint8_t {Jan=0, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec};

// structure to describe rules for when daylight/summer-time begins, or when standard time begins.
struct TimeChangeRule {
    char name[6];      // short name, five chars max
    week_t week;      // First, Second, Third, Fourth, or Last week of the month
    dow_t dow;       // day of week, 0=Sun, 1=Mon, ... 6=Sat
    month_t month;     // 0=Jan, 1=Feb, ... 11=Dec
    uint8_t hour;      // 0-23
    int offsetMin;        // offset from UTC in minutes (more user-friendly)
};
struct dstTransitions {
    time_t m_dstUTC{};        // dst start for the given year, given in UTC (seconds)
    time_t m_stdUTC{};        // std time start for given year, given in UTC (seconds)
    time_t m_dstLoc{};        // dst start for the given year, given in local time (seconds)
    time_t m_stdLoc{};        // std time start for given year, given in local time (seconds)
    uint16_t m_year{};        // year of the update
};

/**
 * @class Timezone
 * @brief Represents a time zone, including rules for daylight saving time (DST) and standard time.
 *
 * The Timezone class provides thread-safe methods to handle time conversions, determine whether a time
 * is in DST or standard time, and to manage time zone rules.
 */
class Timezone {
    public:
        Timezone(const TimeChangeRule &dstStart, const TimeChangeRule &stdStart, const char *name);
        Timezone(const TimeChangeRule &stdTime, const char *name);
        [[nodiscard]] time_t toLocal(const time_t &utc);
        [[nodiscard]] time_t toUTC(const time_t &local);
        [[nodiscard]] bool isDST(const time_t &time, bool bLocal = true);
        void updateZoneInfo(tmElements_t &tm, const time_t &time);
        /** The full IANA time zone name */
        [[nodiscard]] const char *getName() const { return m_name; }
        /** The short zone name for the given local time_t */
        [[nodiscard]] const char *getShort(const time_t local) { return isDST(local) ? m_dst.name : m_std.name; }
        /** The Daylight Savings Time short zone name */
        [[nodiscard]] const char *getDSTShort() const { return m_dst.name; }
        /** The Standard Time short zone name */
        [[nodiscard]] const char *getSTDShort() const { return m_std.name; }
        /** daylight savings time offset from UTC in seconds (standard unit) */
        [[nodiscard]] int getDSTOffset() const { return m_dst.offsetMin*static_cast<int>(SECS_PER_MIN); }
        /** standard time offset from UTC in seconds (standard unit) */
        [[nodiscard]] int getSTDOffset() const { return m_std.offsetMin*static_cast<int>(SECS_PER_MIN); }
        /** local time_t offset from UTC in seconds (standard unit) */
        [[nodiscard]] int getOffset(const time_t time, const bool bLocal=true) { return (isDST(time, bLocal) ? m_dst.offsetMin : m_std.offsetMin)*static_cast<int>(SECS_PER_MIN); }
        /** Whether this time zone observes DST */
        [[nodiscard]] bool isDSTObserved() const { return m_dst.offsetMin != m_std.offsetMin; }

    protected:
        void calcTimeChanges(int year);

    private:
        char m_name[33]{};        // name of the timezone, e.g. "America/New_York", max 32 chars
        const TimeChangeRule m_dst;     // rule for the start of dst or summer-time for any year
        const TimeChangeRule m_std;     // rule for start of standard time for any year
        mutex_t mutex;
        dstTransitions currentTransitions;  // current DST transitions for the last year inquired
};

extern Timezone utcZone;
#endif