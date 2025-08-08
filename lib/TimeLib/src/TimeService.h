// MIT License
//
// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef TIMESERVICE_H
#define TIMESERVICE_H

#include "TimeDef.h"
#include "NTPClient.h"
#include "Timezone.h"
#include "CoreTimeCalc.h"

// function declaration prototype that provides the platform's system hardware clock in milliseconds since boot
typedef time_t (*getSystemLocalClock)();

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
int localWeekday(time_t t); // the weekday for the given time in local timezone (1-7, Sunday is 1)
int localMonth(time_t t); // the month for the given time in local timezone (1-12, Jan is 1)
int localYear(time_t t); // the year for the given time in local timezone
int localDayOfYear(time_t t); // the day of the year for the given time in local timezone (1-366)

time_t now();           // return the current local time as seconds since Jan 1 1970 (aka local unix time)
time_t nowMillis();     // return the current local time as milliseconds since Jan 1 1970
time_t utcNow();        // return the current UTC time as seconds since Jan 1 1970 (aka unix time)
time_t utcNowMillis();  // return the current UTC time as milliseconds since Jan 1 1970

class TimeService {
    NTPClient ntpClient;
    getSystemLocalClock getLocalClockMillisFunc;
    time_t syncUnixMillis {0};    // the absolute time - matching syncLocalMillis - when this cache was updated last. In UTC millis since 1/1/1970
    time_t syncLocalMillis {0};   // the last cached millis() value
    time_t drift {0};           // the current drift adjustment in milliseconds (to be applied as a correction to absolute time)
    Timezone *tz {};

public:
    explicit TimeService(getSystemLocalClock platformTime=nullptr);
    explicit TimeService(UDP& udp, getSystemLocalClock platformTime=nullptr);
    TimeService(UDP& udp, const char* poolServerName, getSystemLocalClock platformTime=nullptr);
    TimeService(UDP& udp, const IPAddress &poolServerIP, getSystemLocalClock platformTime=nullptr);

    void begin(UDP* udp = nullptr);

    void setTime(time_t t);
    time_t setTime(uint16_t hr, uint16_t min, uint16_t sec, uint16_t day, uint16_t month, int year, int offset);

    time_t addDrift(long adjustment);
    time_t setDrift(long adjustment);
    [[nodiscard]] time_t getDrift() const { return drift; }

    void applyTimezone(Timezone &tZone);
    [[nodiscard]] Timezone* timezone() const { return tz; }

    void end();

    /* time sync functions	*/
    bool syncTimeNTP();
    [[nodiscard]] time_t syncLocalTimeMillis() const { return syncLocalMillis; }
    [[nodiscard]] time_t syncUTCTimeMillis() const { return syncUnixMillis; }
    [[nodiscard]] time_t utcFromRtcMillis(const time_t &rtcMillis) const;
    [[nodiscard]] time_t localFromRtcMillis(const time_t &rtcMillis) const;

    void breakTime(const time_t &timeInput, tmElements_t &tmItems) const; // break time_t into elements with timezone
    /** Break the given local time_t (seconds since 1/1/1970) into time components without timezone adjustments */
    static void breakTimeNoTZ(const time_t &timeInput, tmElements_t &tmItems) { CoreTimeCalc::breakTimeCore(timeInput, tmItems); }; // break time_t into elements without a timezone
    static time_t makeTime(const tmElements_t &tmItems); // convert time elements into time_t
    static time_t makeTimeNoTZ(const tmElements_t &tmItems); // convert time elements into time_t without timezone

    friend time_t nowMillis();       // return the current local time as milliseconds since Jan 1 1970
    friend time_t utcNowMillis();    // return the current UTC time as milliseconds since Jan 1 1970
};

// needs defined by the user and provide the UDP implementation and optional NTP sync details
extern TimeService timeService;

#endif //TIMESERVICE_H
