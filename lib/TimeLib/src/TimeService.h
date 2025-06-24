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

#define DEFAULT_SYNC_INTERVAL (12 * SECS_PER_HOUR)  // default interval in seconds between NTP sync attempts

typedef enum {
    timeNotSet, timeNeedsSync, timeSet
} timeSyncStatus_t;

typedef tm tmElements_t;
// function declaration prototype that provides the platform's system hardware clock in milliseconds since boot
typedef time_t (*getSystemLocalClock)();

/*============================================================================*/
/*  global time and date functions   */
int hour(); // the hour now
int hour(time_t t); // the hour for the given time

int hourFormat12(); // the hour now in 12-hour format
int hourFormat12(time_t t); // the hour for the given time in 12-hour format

uint8_t isAM(); // returns true if time now is AM
uint8_t isAM(time_t t); // returns true the given time is AM

uint8_t isPM(); // returns true if time now is PM
uint8_t isPM(time_t t); // returns true the given time is PM

int minute(); // the minute now
int minute(time_t t); // the minute for the given time

int second(); // the second now
int second(time_t t); // the second for the given time

int day(); // the day now
int day(time_t t); // the day for the given time

int weekday(); // the weekday now (Sunday is day 1)
int weekday(time_t t); // the weekday for the given time

int month(); // the month now (Jan is month 1)
int month(time_t t); // the month for the given time

int year(); // the full four-digit year: (2009, 2010 etc)
int year(time_t t); // the year for the given time

int dayOfYear(); // the day of the year (1-366)
int dayOfYear(time_t t); // the day of the year for the given time

time_t now();           // return the current local time as seconds since Jan 1 1970 (aka local unix time)
time_t nowMillis();     // return the current local time as milliseconds since Jan 1 1970
time_t utcNow();        // return the current UTC time as seconds since Jan 1 1970 (aka unix time)
time_t utcNowMillis();  // return the current UTC time as milliseconds since Jan 1 1970

class TimeService {
    NTPClient ntpClient;
    getSystemLocalClock getLocalClockMillisFunc;
    timeSyncStatus_t status {timeNotSet};
    time_t syncUnixMillis {0};    // the absolute time - matching syncLocalMillis - when this cache was updated last. In UTC millis since 1/1/1970
    time_t syncLocalMillis {0};   // the last cached millis() value
    time_t drift {0};           // the current drift adjustment in milliseconds (to be applied as a correction to absolute time)
    unsigned long syncInterval = DEFAULT_SYNC_INTERVAL;  // time cache update will be attempted after this many seconds;
    const Timezone *tz {};

public:
    explicit TimeService(UDP& udp, getSystemLocalClock platformTime=nullptr);
    TimeService(UDP& udp, const char* poolServerName, unsigned long updateInterval=0, getSystemLocalClock platformTime=nullptr);
    TimeService(UDP& udp, const IPAddress &poolServerIP, unsigned long updateInterval=0, getSystemLocalClock platformTime=nullptr);

    void begin();

    void setTime(time_t t);
    time_t setTime(int hr, int min, int sec, int day, int month, int year, int offset);

    time_t addDrift(long adjustment);
    time_t setDrift(long adjustment);
    [[nodiscard]] time_t getDrift() const { return drift; }

    void applyTimezone(const Timezone &tZone);
    [[nodiscard]] const Timezone* timezone() const { return tz; }

    [[nodiscard]] timeSyncStatus_t timeStatus() const { return status; };

    void end();

    /* time sync functions	*/
    bool syncTimeNTP();
    void setSyncInterval(time_t interval); // set the number of seconds between re-sync
    [[nodiscard]] time_t syncLocalTimeMillis() const { return syncLocalMillis; }
    [[nodiscard]] time_t syncUTCTimeMillis() const { return syncUnixMillis; }

    void breakTime(time_t timeInput, tmElements_t &tmItems) const; // break time_t into elements
    static time_t makeTime(const tmElements_t &tmItems); // convert time elements into time_t

    friend time_t nowMillis();       // return the current local time as milliseconds since Jan 1 1970
    friend time_t utcNowMillis();    // return the current UTC time as milliseconds since Jan 1 1970
};

// needs defined by the user and provide the UDP implementation and optional NTP sync details
extern TimeService timeService;

#endif //TIMESERVICE_H
