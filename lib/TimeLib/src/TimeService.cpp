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
#include <atomic>
#include <cinttypes>
#include <sys/time.h>
#include <WiFi.h>
#include "TimeService.h"
#include "Timezone.h"
#include "TimeLib.h"

// The framework's system clock hook (cores/rp2040/posix.cpp) - sets the clock behind gettimeofday(). The lwIP SNTP client calls it through
// the wrap below (linker option -Wl,--wrap=__setSystemTime in platformio.ini), so the framework implementation is __real___setSystemTime.
extern "C" void __real___setSystemTime(unsigned long long sec, unsigned long usec);

constexpr uint8_t monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31}; // API starts months from 1, this array starts from 0

// SNTP state - written from the lwIP context (the sync hook below), read from any task
static std::atomic<bool> ntpRunning {false};
static std::atomic<uint32_t> ntpSyncCount {0};
static std::atomic<uint32_t> ntpRejectedCount {0};
static std::atomic<uint32_t> ntpLastSyncUtc {0};    // unix seconds fit 32 bits until 2106

/**
 * Replaces __setSystemTime for the lwIP SNTP client (SNTP_SET_SYSTEM_TIME_US), which calls it every time it receives a time from the server;
 * the linker redirects the references with -Wl,--wrap=__setSystemTime.
 * Runs in the lwIP context - keep it short, no logging, no blocking calls.
 * Sets the system clock only when the time is within the valid range.
 * @param sec seconds since 1/1/1970
 * @param usec microseconds
 */
extern "C" void __wrap___setSystemTime(const unsigned long long sec, const unsigned long usec) {
    if (sec < static_cast<unsigned long long>(TIME_VALID_MIN) || sec > static_cast<unsigned long long>(TIME_VALID_MAX)) {
        ntpRejectedCount.fetch_add(1);
        return;
    }
    __real___setSystemTime(sec, usec);
    ntpLastSyncUtc.store(static_cast<uint32_t>(sec));
    ntpSyncCount.fetch_add(1);
}

/**
 * Milliseconds since boot, 64 bits (millis() wraps after 49 days)
 */
static time_t bootMillis() {
  return static_cast<time_t>(to_us_since_boot(get_absolute_time()) / 1000ULL);
}

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
 * @param timeInput time input to convert - local seconds since 1/1/1970
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
  const time_t offsetMillis = static_cast<time_t>(timeService.timezone()->getOffset(utcMillis / 1000, false)) * 1000;
  return utcMillis + offsetMillis;
}

/**
 * The current UTC time in milliseconds, read from the system clock (kept in sync by the lwIP SNTP client).
 * Until the first sync, the system clock counts from the epoch at boot.
 *
 * @return The current UTC time in milliseconds since January 1, 1970.
 */
time_t utcNowMillis() {
  timeval tv{};
  gettimeofday(&tv, nullptr);
  return static_cast<time_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

/**
 * Converts the given boot-relative timestamp in milliseconds to UTC time in milliseconds.
 * The system clock is boot-relative plus a delta that only changes when the clock is set, so the translation
 * is the current delta - correct for timestamps captured since the last clock change (e.g. before the first sync).
 * @param rtcMillis milliseconds since boot
 * @return The corresponding UTC time in milliseconds since January 1, 1970.
 */
time_t TimeService::utcFromRtcMillis(const time_t &rtcMillis) const {
  return rtcMillis + (utcNowMillis() - bootMillis());
}

/**
 * Converts the given boot-relative timestamp in milliseconds to local time in milliseconds, accounting for timezone offsets.
 *
 * @param rtcMillis milliseconds since boot
 * @return The equivalent local time as time_t in milliseconds since the epoch.
 */
time_t TimeService::localFromRtcMillis(const time_t &rtcMillis) const {
  const time_t utcMillis = utcFromRtcMillis(rtcMillis);
  const time_t offsetMillis = static_cast<time_t>(tz->getOffset(utcMillis / 1000, false)) * 1000;
  return utcMillis + offsetMillis;
}

/**
 * Sets the system clock from a UTC value
 * @param t current UTC time to set - seconds since 1/1/1970
 */
void TimeService::setTime(const time_t t) {
  __real___setSystemTime(static_cast<unsigned long long>(t), 0);    // a manual set is not an NTP sync - bypasses the SNTP hook
}

/**
 * Sets the system clock to the value built from individual elements
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
  tm.tm_year = year - TM_EPOCH_YEAR;  // makeTime interprets tm_year as years since 1900
  tm.tm_mon = month == 0 ? month : month-1;  //per https://en.cppreference.com/w/c/chrono/tm.html the tm_mon field uses 0-11 range
  tm.tm_mday = day;
  tm.tm_hour = hr;
  tm.tm_min = min;
  tm.tm_sec = sec;
  tm.tm_offset = offset;
  const time_t t = makeTime(tm);
  setTime(t);
  return t;
}

/**
 * Sets the time zone for local time and installs it in the C library.
 * @param tZone the new timezone to apply - must outlive the time service (use a static/global object)
 */
void TimeService::applyTimezone(Timezone &tZone) {
  tZone.apply();
  tz = &tZone;
}

bool TimeService::beginNTP(const IPAddress &server) {
  if (!server.isSet())
    return false;
  if (!ntpRunning.exchange(true))
    NTP.begin(server);
  return true;
}

bool TimeService::beginNTP(const char *serverName) {
  if (ntpRunning.load())
    return true;
  IPAddress addr;
  if (!::hostByName(serverName, addr))
    return false;
  return beginNTP(addr);
}

void TimeService::endNTP() {
  if (ntpRunning.exchange(false))
    sntp_stop();
}

bool TimeService::isNtpRunning() const {
  return ntpRunning.load();
}

bool TimeService::isSynchronized() const {
  return ntpSyncCount.load() > 0;
}

bool TimeService::waitSynchronized(const uint32_t timeoutMs) const {
  const uint32_t start = millis();
  while (!isSynchronized() && (millis() - start) < timeoutMs)
    delay(100);
  return isSynchronized();
}

uint32_t TimeService::syncCount() const {
  return ntpSyncCount.load();
}

time_t TimeService::lastSyncUTC() const {
  return static_cast<time_t>(ntpLastSyncUtc.load());
}

time_t TimeService::lastSyncAge() const {
  if (ntpSyncCount.load() == 0)
    return -1;
  return utcNow() - lastSyncUTC();
}

uint32_t TimeService::rejectedCount() const {
  return ntpRejectedCount.load();
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

