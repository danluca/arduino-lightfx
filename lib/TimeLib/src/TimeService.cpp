/**
  time.c - low level time and date functions
  Copyright (c) Michael Margolis 2009-2014

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  1.0  6  Jan 2010 - initial release
  1.1  12 Feb 2010 - fixed leap year calculation error
  1.2  1  Nov 2010 - fixed setTime bug (thanks to Korman for this)
  1.3  24 Mar 2012 - many edits by Paul Stoffregen: fixed timeStatus() to update
                     status, updated examples for Arduino 1.0, fixed ARM
                     compatibility issues, added TimeArduinoDue and TimeTeensy3
                     examples, add error checking and messages to RTC examples,
                     add examples to DS1307RTC library.
  1.4  5  Sep 2014 - compatibility with Arduino 1.5.7
*/

#include <Arduino.h>
#include <cinttypes>
#include "TimeService.h"
#include "Timezone.h"
#include "TimeLib.h"
#include "LogProxy.h"
#ifdef PICO_RP2040
#include <hardware/rtc.h>
#include "pico/util/datetime.h"
#endif

//TODO: ? (this is default implementation already in the platform code)
// time_t pico_mktime(struct tm *tm) {
  // return mktime(tm);
// }

/**
 * Default implementation for getSystemLocalClock prototype
 * @return milliseconds since boot
 */
time_t defaultPlatformBootTimeMillis() {
  return millis();
}

constexpr uint8_t monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31}; // API starts months from 1, this array starts from 0

// the hour now
int hour() {
  return hour(now()); 
}
// the hour for the given time
int hour(const time_t t) {
  return CoreTimeCalc::hourCore(t);
}
// the hour now in 12-hour format
int hourFormat12() {
  return hourFormat12(now()); 
}
// the hour for the given time in 12-hour format
int hourFormat12(const time_t t) {
  tmElements_t tm{};
  timeService.breakTime(t, tm);
  if( tm.tm_hour == 0 )
    return 12; // 12 midnight
  if( tm.tm_hour  > 12)
    return tm.tm_hour - 12 ;
  return tm.tm_hour ;
}
// returns true if time now is AM
uint8_t isAM() {
  return !isPM(now()); 
}
// returns true if the given time is AM
uint8_t isAM(const time_t t) {
  return !isPM(t);  
}
// returns true if PM
uint8_t isPM() {
  return isPM(now()); 
}
// returns true if PM
uint8_t isPM(const time_t t) {
  return hour(t) >= 12;
}
// the minute now (0-59)
int minute() {
  return minute(now()); 
}
// the minute for the given time (0-59)
int minute(const time_t t) {
  return CoreTimeCalc::minuteCore(t);
}
// the second now (0-59)
int second() {
  return second(now()); 
}
// the second for the given time (0-59)
int second(const time_t t) {
  return CoreTimeCalc::secondCore(t);
}
// the day now (1-31)
int day(){
  return(day(now())); 
}
// the day for the given time (1-31)
int day(const time_t t) {
  return CoreTimeCalc::dayCore(t);
}
// the week day now (0-6), Sunday is 0
int weekday() {
  return weekday(now());
}
// the week day for the given time (0-6), Sunday is 0
int weekday(const time_t t) {
  return CoreTimeCalc::weekdayCore(t);
}
// the month now (0-11)
int month(){
  return month(now()); 
}
// the month for the given time
int month(const time_t t) {
  return CoreTimeCalc::monthCore(t);
}
// the full four-digit year: (2009, 2010, etc)
int year() {
  return year(now()); 
}
// the year for the given time
int year(const time_t t) {
  return CoreTimeCalc::yearCore(t);
}
// the day of the year for now (1-366)
int dayOfYear() {
  return dayOfYear(now());
}
// the day of the year for the given time (1-366)
int dayOfYear(const time_t t) {
  return CoreTimeCalc::dayOfYearCore(t);
}

/**
 * Break the given local time_t into time components
 * See https://en.cppreference.com/w/c/chrono/tm.html for time elements values/ranges
 * @param timeInput time input to convert - seconds since 1/1/1970
 * @param tmItems the time structure to fill broken time fields into
 */
void TimeService::breakTime(const time_t &timeInput, tmElements_t &tmItems) const {
  // First, do the core time breakdown without timezone
  CoreTimeCalc::breakTimeCore(timeInput, tmItems);

  // Then apply timezone adjustments
  tz->updateZoneInfo(tmItems, timeInput);
}

/**
 * Assemble time elements into local time_t seconds since 1/1/1970
 * See https://en.cppreference.com/w/c/chrono/tm.html for time elements values/ranges
 * @param tmItems time structure to assemble
 * @return local time corresponding to time elements
 */
time_t TimeService::makeTime(const tmElements_t &tmItems) {
  // Use core time calculation and add timezone offset
  time_t seconds = CoreTimeCalc::makeTimeCore(tmItems);
  // Apply timezone offset if provided
  seconds += tmItems.tm_offset;
  return seconds;
}

/**
 * Assemble time elements into time_t seconds since 1/1/1970 without timezone adjustments
 * @param tmItems time structure to assemble
 * @return time corresponding to time elements without timezone adjustments
 */
time_t TimeService::makeTimeNoTZ(const tmElements_t &tmItems) {
  // Just use the core time calculation without timezone offset
  return CoreTimeCalc::makeTimeCore(tmItems);
}

/**
 * current (local) time, in seconds since 1970; includes offset adjustments for timezone
 * @return
 */
time_t now() {
  return nowMillis() / 1000;
}

time_t utcNow() {
  return utcNowMillis() / 1000;
}

/**
 * current (local) time, in milliseconds since 1970; includes offset adjustments for timezone
 * @return
 */
time_t nowMillis() {
  const time_t utcMillis = utcNowMillis();
  const time_t offsetMillis = timeService.tz->getOffset(utcMillis / 1000, false) * 1000;
  return utcMillis + offsetMillis;
}

/**
 * Calculates the current UTC time in milliseconds.
 * It computes the value based on the system clock, the synchronization offsets,
 * and the drift adjustment.
 *
 * @return The current UTC time in milliseconds since January 1, 1970.
 */
time_t utcNowMillis() {
  const time_t sysClock = timeService.getLocalClockMillisFunc();  // current milliseconds since boot
  const time_t utcMillis = (sysClock - timeService.syncLocalMillis) + timeService.syncUnixMillis + timeService.drift;  //current UTC time in millis
  return utcMillis;
}

/**
 * Converts the given RTC timestamp in milliseconds to UTC time in milliseconds.
 * The calculation adjusts the provided rtcMillis value using the current synchronized
 * local time (syncLocalMillis), the synchronized UTC time (syncUnixMillis),
 * and the applied drift correction.
 *
 * This method operates similarly to utcNowMillis but avoids additional function
 * calls for improved efficiency by reducing stack depth.
 *
 * @param rtcMillis A reference to the time in milliseconds, as retrieved from the RTC.
 * @return The corresponding UTC time in milliseconds since January 1, 1970.
 */
time_t TimeService::utcFromRtcMillis(const time_t &rtcMillis) const {
  // similar logic with the utcNowMillis - repeated rather than doing a function call for efficiency (less stack depth)
  return (rtcMillis - syncLocalMillis) + syncUnixMillis + drift;  //current UTC time in millis
}

/**
 * Converts the given real-time clock (RTC) time in milliseconds since the epoch
 * to local time accounting for timezone offsets.
 *
 * @param rtcMillis A reference to the time_t value representing RTC milliseconds since the epoch.
 * @return The equivalent local time as time_t in milliseconds since the epoch.
 */
time_t TimeService::localFromRtcMillis(const time_t &rtcMillis) const {
  // similar logic with nowMillis
  const time_t utcMillis = utcFromRtcMillis(rtcMillis);
  const time_t offsetMillis = tz->getOffset(utcMillis / 1000, false) * 1000;
  return utcMillis + offsetMillis;
}

/**
 * Sets the current time from a UTC value
 * @param t current UTC time to set - seconds since 1/1/1970
 */
void TimeService::setTime(const time_t t) {
  const time_t sysClock = getLocalClockMillisFunc();
  syncLocalMillis = sysClock;
  syncUnixMillis = t * 1000;  //we store the absolute time in milliseconds
#ifdef PICO_RP2040
  tmElements_t tm{};
  breakTime(t, tm);
  datetime_t dt;
  rtc_get_datetime(&dt);
  const bool timeNeedsUpdate = dt.year != tm.tm_year || dt.month != (tm.tm_mon+1) || dt.day != tm.tm_mday ||
                          dt.hour != tm.tm_hour || dt.min != tm.tm_min || dt.sec != tm.tm_sec;

  if (timeNeedsUpdate) {
    dt.year = static_cast<int16_t>(tm.tm_year);
    dt.month = static_cast<int8_t>(tm.tm_mon+1);
    dt.day = static_cast<int8_t>(tm.tm_mday);
    dt.hour = static_cast<int8_t>(tm.tm_hour);
    dt.min = static_cast<int8_t>(tm.tm_min);
    dt.sec = static_cast<int8_t>(tm.tm_sec);
    rtc_set_datetime(&dt);
  }
#endif

}

/**
 * Sets the current time to the value built from individual elements
 * Note: Generally, no validity checks are performed on the input data
 * @param hr the hour of the day - 0-23
 * @param min the minute of the hour - 0-59
 * @param sec the second of the minute - 0-59
 * @param day the day of the month - 1-31 (no checks are made whether the day is a valid value for the month)
 * @param month the month of the year - 1-12
 * @param year the four-digit year
 * @param offset the time offset from UTC in seconds
 * @return the time_t built from these elements
 */
time_t TimeService::setTime(const uint16_t hr, const uint16_t min, const uint16_t sec, const uint16_t day, const uint16_t month, const int year, const int offset) {
  tmElements_t tm{};
  tm.tm_year = year;
  tm.tm_mon = month == 0 ? month : month-1;  //per https://en.cppreference.com/w/c/chrono/tm.html the tm_mon field uses 0-11 range
  tm.tm_mday = day;
  tm.tm_hour = hr;
  tm.tm_min = min;
  tm.tm_sec = sec;
  tm.tm_offset = offset;
  const time_t t = makeTime(tm);
  //inlined the setTime(time_t) to avoid a redundant time break for RP2040
  const time_t sysClock = getLocalClockMillisFunc();
  syncLocalMillis = sysClock;
  syncUnixMillis = t * 1000;  //we store the absolute time in milliseconds
#ifdef PICO_RP2040
  datetime_t dt;
  rtc_get_datetime(&dt);
  const bool timeNeedsUpdate = dt.year != tm.tm_year || dt.month != (tm.tm_mon+1) || dt.day != tm.tm_mday ||
                          dt.hour != tm.tm_hour || dt.min != tm.tm_min || dt.sec != tm.tm_sec;

  if (timeNeedsUpdate) {
    dt.year = static_cast<int16_t>(tm.tm_year);
    dt.month = static_cast<int8_t>(tm.tm_mon+1);
    dt.day = static_cast<int8_t>(tm.tm_mday);
    dt.hour = static_cast<int8_t>(tm.tm_hour);
    dt.min = static_cast<int8_t>(tm.tm_min);
    dt.sec = static_cast<int8_t>(tm.tm_sec);
    rtc_set_datetime(&dt);
  }
#endif
  return t;
}

/**
 * Increments the drift offset with the millisecond value provided
 * @param adjustment time drift adjustment in milliseconds
 * @return previous drift value
 */
time_t TimeService::addDrift(const long adjustment) {
  const time_t prevDrift = drift;
  drift += adjustment;
  return prevDrift;
}

/**
 * Set the drift offset to the millisecond value provided
 * @param adjustment time drift adjustment in milliseconds
 * @return previous drift value
 */
time_t TimeService::setDrift(const long adjustment) {
  const time_t prevDrift = drift;
  drift = adjustment;
  return prevDrift;
}

/**
 *
 * @param tZone the new timezone rule to apply
 */
void TimeService::applyTimezone(Timezone &tZone) {
  tz = &tZone;
}

TimeService::TimeService(getSystemLocalClock platformTime) {
  if (platformTime == nullptr) {
    platformTime = defaultPlatformBootTimeMillis;
  }
  getLocalClockMillisFunc = platformTime;
  syncLocalMillis = 0;
  syncUnixMillis = 0;
  drift = 0;
  tz = &utcZone;
}

TimeService::TimeService(UDP& udp, getSystemLocalClock platformTime): ntpClient(udp) {
  if (platformTime == nullptr) {
    platformTime = defaultPlatformBootTimeMillis;
  }
  getLocalClockMillisFunc = platformTime;
  syncLocalMillis = 0;
  syncUnixMillis = 0;
  drift = 0;
  tz = &utcZone;
}

TimeService::TimeService(UDP& udp, const char* poolServerName, getSystemLocalClock platformTime): ntpClient(udp, poolServerName) {
  if (platformTime == nullptr) {
    platformTime = defaultPlatformBootTimeMillis;
  }
  getLocalClockMillisFunc = platformTime;
  syncLocalMillis = 0;
  syncUnixMillis = 0;
  drift = 0;
  tz = &utcZone;
}

TimeService::TimeService(UDP& udp, const IPAddress &poolServerIP, getSystemLocalClock platformTime): ntpClient(udp, poolServerIP) {
  if (platformTime == nullptr)
    platformTime = defaultPlatformBootTimeMillis;
  getLocalClockMillisFunc = platformTime;
  syncLocalMillis = 0;
  syncUnixMillis = 0;
  drift = 0;
  tz = &utcZone;
}

void TimeService::begin(UDP* udp) {
    ntpClient.begin(udp);
#ifdef PICO_RP2040
  if (!rtc_running())
    rtc_init();
#endif
}

/**
 * If the NTP time sync is due, perform NTP time synchronization
 * @return whether the NTP sync was successful and decoded valid time
 */
bool TimeService::syncTimeNTP() {
  time_t epochTime = 0;
  int delay = 0; //milliseconds delay in processing the time data received; accounts for network lag
  const bool success = ntpClient.update(epochTime, delay);
  if (success) {
    setTime(epochTime);
    syncLocalMillis -= delay; //the sys millis is set to now in the call above, adjust it with the delay reported by the NTP service
  }
  return success;
}

void TimeService::end() {
  ntpClient.end();
}

// Timezone-aware versions of time component functions
int localHour(const time_t t) {
  tmElements_t tm{};
  timeService.breakTime(t, tm); // Use the timezone-aware version
  return tm.tm_hour;
}

int localMinute(const time_t t) {
  tmElements_t tm{};
  timeService.breakTime(t, tm);
  return tm.tm_min;
}

int localSecond(const time_t t) {
  tmElements_t tm{};
  timeService.breakTime(t, tm);
  return tm.tm_sec;
}

int localDay(const time_t t) {
  tmElements_t tm{};
  timeService.breakTime(t, tm);
  return tm.tm_mday;
}

int localWeekday(const time_t t) {
  tmElements_t tm{};
  timeService.breakTime(t, tm);
  return tm.tm_wday;
}

int localMonth(const time_t t) {
  tmElements_t tm{};
  timeService.breakTime(t, tm);
  return tm.tm_mon;
}

int localYear(const time_t t) {
  tmElements_t tm{};
  timeService.breakTime(t, tm);
  return tm.tm_year;
}

int localDayOfYear(const time_t t) {
  tmElements_t tm{};
  timeService.breakTime(t, tm);
  return tm.tm_yday;
}

