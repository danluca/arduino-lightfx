// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//
#pragma once
#ifndef TIMESERVICE_H
#define TIMESERVICE_H

#include "TimeDef.h"
#include "Timezone.h"
#include "CoreTimeCalc.h"

/**
 * Bounds for a sane wall clock time: the time source is considered valid between 2020-01-01 and 2070-01-01 (seconds since the epoch).
 * Used to reject bogus SNTP replies (e.g. year 2100) and to tell a synchronized clock from the boot-relative one.
 */
constexpr time_t TIME_VALID_MIN = 1577836800L;      // 2020-01-01 00:00 UTC
constexpr time_t TIME_VALID_MAX = 3155760000L;      // 2070-01-01 00:00 UTC - if this code is still relevant in 2070, something is wrong...

/*============================================================================*/
/*  global time and date functions   */
int hour(); // the hour now (0-23)
int hour(time_t t); // the hour for the given time (0-23)

int hourFormat12(); // the hour now in 12-hour format (1-12)
int hourFormat12(time_t t); // the hour for the given time in 12-hour format (1-12)

uint8_t isAM(); // returns true if time now is AM
uint8_t isAM(time_t t); // returns true the given time is AM

uint8_t isPM(); // returns true if time now is PM
uint8_t isPM(time_t t); // returns true the given time is PM

int minute(); // the minute now (0-59)
int minute(time_t t); // the minute for the given time (0-59)

int second(); // the second now (0-59)
int second(time_t t); // the second for the given time (0-59)

int day(); // the day now (1-31)
int day(time_t t); // the day for the given time (1-31)

int weekday(); // the weekday now (0-6, Sunday is day 0)
int weekday(time_t t); // the weekday for the given time (0-6, Sunday is day 0)

int month(); // the month now (0-11, Jan is month 0)
int month(time_t t); // the month for the given time (0-11, Jan is month 0)

int year(); // the full four-digit year: (2009, 2010 etc)
int year(time_t t); // the year for the given time

int dayOfYear(); // the day of the year (1-366)
int dayOfYear(time_t t); // the day of the year for the given time (1-366)

// Timezone-aware versions of time component functions
int localHour(time_t t); // the hour for the given time in local timezone (0-23)
int localMinute(time_t t); // the minute for the given time in local timezone (0-59)
int localSecond(time_t t); // the second for the given time in local timezone (0-59)
int localDay(time_t t); // the day for the given time in local timezone (1-31)
int localWeekday(time_t t); // the weekday for the given time in local timezone (0-6, Sunday is 0)
int localMonth(time_t t); // the month for the given time in local timezone (1-12, Jan is 1)
int localYear(time_t t); // the year for the given time in local timezone
int localDayOfYear(time_t t); // the day of the year for the given time in local timezone (1-366)

time_t now();           // return the current local time as seconds since Jan 1 1970 (aka local unix time)
time_t nowMillis();     // return the current local time as milliseconds since Jan 1 1970
time_t utcNow();        // return the current UTC time as seconds since Jan 1 1970 (aka unix time)
time_t utcNowMillis();  // return the current UTC time as milliseconds since Jan 1 1970

/**
 * Time facade over the platform primitives:
 * - the UTC clock is the system clock (gettimeofday), set by the lwIP SNTP client - no sync bookkeeping or drift correction here
 * - the local time is derived from the system clock with the C library time zone support, see Timezone
 * - NTP is the lwIP SNTP client; it re-polls on its own schedule (SNTP_UPDATE_DELAY, a build flag in platformio.ini) and steps the system clock
 *   through __setSystemTime, which the linker redirects (-Wl,--wrap=__setSystemTime) to a hook here that validates the time and records the sync
 */
class TimeService {
    Timezone *tz {&utcZone};

public:
    TimeService() = default;

    /** Sets the time zone used for local time. Installs the zone in the C library - call once, early, before other tasks rely on local time */
    void applyTimezone(Timezone &tZone);
    [[nodiscard]] Timezone* timezone() const { return tz; }

    /** Sets the system clock to the given UTC time (seconds since 1/1/1970) */
    void setTime(time_t t);
    /** Sets the system clock to the time built from individual elements (see makeTime) and returns it as UTC time_t */
    time_t setTime(uint16_t hr, uint16_t min, uint16_t sec, uint16_t day, uint16_t month, int year, int offset);

    /* NTP - lwIP SNTP client */
    /** Starts the SNTP client against the given server (single server). No-op if already started; returns true if running afterwards. */
    bool beginNTP(const IPAddress &server);
    /** Resolves the host name (blocking DNS lookup) and starts the SNTP client. Returns true if running afterwards. */
    bool beginNTP(const char *serverName);
    /** Stops the SNTP client; the system clock keeps running free from its last sync */
    void endNTP();
    [[nodiscard]] bool isNtpRunning() const;
    /** Whether the system clock was set (and validated) by SNTP at least once since boot */
    [[nodiscard]] bool isSynchronized() const;
    /** Blocks (polling) until the first SNTP sync or the timeout elapses; returns whether the clock is synchronized */
    bool waitSynchronized(uint32_t timeoutMs) const;
    /** Number of SNTP syncs accepted since boot */
    [[nodiscard]] uint32_t syncCount() const;
    /** UTC time (seconds since 1/1/1970) of the most recent accepted SNTP sync; 0 if there was none */
    [[nodiscard]] time_t lastSyncUTC() const;
    /** Seconds since the most recent accepted SNTP sync; -1 if there was none */
    [[nodiscard]] time_t lastSyncAge() const;
    /** Number of SNTP replies rejected as outside the valid time range */
    [[nodiscard]] uint32_t rejectedCount() const;

    /** Converts a boot-relative timestamp (milliseconds since boot, e.g. millis() captured before the first sync) to UTC milliseconds since 1/1/1970 */
    [[nodiscard]] time_t utcFromRtcMillis(const time_t &rtcMillis) const;
    /** Converts a boot-relative timestamp (milliseconds since boot) to local milliseconds since 1/1/1970 */
    [[nodiscard]] time_t localFromRtcMillis(const time_t &rtcMillis) const;

    void breakTime(const time_t &timeInput, tmElements_t &tmItems) const; // break local time_t into elements with timezone
    /** Break the given local time_t (seconds since 1/1/1970) into time components without timezone adjustments */
    static void breakTimeNoTZ(const time_t &timeInput, tmElements_t &tmItems) { CoreTimeCalc::breakTimeCore(timeInput, tmItems); }; // break time_t into elements without a timezone
    static time_t makeTime(const tmElements_t &tmItems); // convert time elements into time_t
    static time_t makeTimeNoTZ(const tmElements_t &tmItems); // convert time elements into time_t without timezone
};

// needs defined by the user
extern TimeService timeService;

#endif //TIMESERVICE_H
