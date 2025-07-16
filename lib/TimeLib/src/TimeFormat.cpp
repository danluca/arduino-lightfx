// MIT License
//
// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include <Arduino.h>
#include "TimeFormat.h"
#include "TimeService.h"

// the short strings for each day or month must be exactly dt_SHORT_STR_LEN
#define dt_SHORT_STR_LEN  3 // the length of short strings
#define dt_MAX_STRING_LEN 9 // length of the longest date string (excluding terminating null)
#define TIME_BUFFER_LENGTH    64

static inline constexpr char monthStr0[] PROGMEM = "Error";
static inline constexpr char monthStr1[] PROGMEM = "January";
static inline constexpr char monthStr2[] PROGMEM = "February";
static inline constexpr char monthStr3[] PROGMEM = "March";
static inline constexpr char monthStr4[] PROGMEM = "April";
static inline constexpr char monthStr5[] PROGMEM = "May";
static inline constexpr char monthStr6[] PROGMEM = "June";
static inline constexpr char monthStr7[] PROGMEM = "July";
static inline constexpr char monthStr8[] PROGMEM = "August";
static inline constexpr char monthStr9[] PROGMEM = "September";
static inline constexpr char monthStr10[] PROGMEM = "October";
static inline constexpr char monthStr11[] PROGMEM = "November";
static inline constexpr char monthStr12[] PROGMEM = "December";

static inline const PROGMEM constexpr char * const PROGMEM monthNames_P[] = {
    monthStr0,monthStr1,monthStr2,monthStr3,monthStr4,monthStr5,monthStr6,
    monthStr7,monthStr8,monthStr9,monthStr10,monthStr11,monthStr12
};

static inline constexpr char monthShortNames_P[] PROGMEM = "ErrJanFebMarAprMayJunJulAugSepOctNovDec";

static inline constexpr char dayStr0[] PROGMEM = "Error";
static inline constexpr char dayStr1[] PROGMEM = "Sunday";
static inline constexpr char dayStr2[] PROGMEM = "Monday";
static inline constexpr char dayStr3[] PROGMEM = "Tuesday";
static inline constexpr char dayStr4[] PROGMEM = "Wednesday";
static inline constexpr char dayStr5[] PROGMEM = "Thursday";
static inline constexpr char dayStr6[] PROGMEM = "Friday";
static inline constexpr char dayStr7[] PROGMEM = "Saturday";

static inline const PROGMEM constexpr char * const PROGMEM dayNames_P[] = {
   dayStr0,dayStr1,dayStr2,dayStr3,dayStr4,dayStr5,dayStr6,dayStr7
};

static inline constexpr char dayShortNames_P[] PROGMEM = "ErrSunMonTueWedThuFriSat";
static inline constexpr auto defaultTimePattern PROGMEM = "%Y-%m-%d %H:%M:%S %z %Z";
static inline constexpr auto defaultTimeMsPattern PROGMEM = "%d-%d-%d %d:%d:%d.%d %+02d:%02d %s";

/**
 * Converts the numeric month value into its string representation - e.g. 2 -> February
 * NOTE: no null termination performed
 * @param month the month value, 1-12, January is 1
 * @param buffer buffer where to write the string representation
 * @return number of characters written
 */
size_t TimeFormat::monthStr(const uint8_t month, char *buffer) {
   PGM_P pgm_ptr = static_cast<const char *>(pgm_read_ptr(&(monthNames_P[month])));
   strcpy_P(buffer, pgm_ptr);
   return strlen_P(pgm_ptr);
}
/**
 * Converts the numeric month value into its short string representation - e.g. 2 -> Feb
 * NOTE: no null termination performed
 * @param month the month value, 1-12, January is 1
 * @param buffer buffer where to write the string representation
 * @return number of characters written
 */
size_t TimeFormat::monthShortStr(const uint8_t month, char *buffer) {
   for (int i=0; i < dt_SHORT_STR_LEN; i++)      
      buffer[i] = pgm_read_byte(&(monthShortNames_P[i+ (month*dt_SHORT_STR_LEN)]));  
   // buffer[dt_SHORT_STR_LEN] = 0;
   return dt_SHORT_STR_LEN;
}

/**
 * Converts the numeric day  value into its string representation - e.g. 2 -> Monday
 * NOTE: no null termination performed
 * @param day the day value, 1-7, Sunday is 1
 * @param buffer buffer where to write the string representation
 * @return number of characters written
 */
size_t TimeFormat::dayStr(const uint8_t day, char *buffer) {
   PGM_P pgm_ptr = static_cast<const char *>(pgm_read_ptr(&(dayNames_P[day])));
   strcpy_P(buffer, pgm_ptr);
   return strlen_P(pgm_ptr);
}

/**
 * Converts the numeric day value into its short string representation - e.g. 2 -> Mon
 * NOTE: no null termination performed
 * @param day the day value, 1-7, Sunday is 1
 * @param buffer buffer where to write the string representation
 * @return number of characters written
 */
size_t TimeFormat::dayShortStr(const uint8_t day, char *buffer) {
   const uint8_t index = day*dt_SHORT_STR_LEN;
   for (int i=0; i < dt_SHORT_STR_LEN; i++)      
      buffer[i] = pgm_read_byte(&(dayShortNames_P[index + i]));  
   // buffer[dt_SHORT_STR_LEN] = 0;
   return dt_SHORT_STR_LEN;
}

size_t TimeFormat::toString(const time_t &time, String &str) {
   const String s = asString(time);
   str.concat(s);
   return s.length();
}

size_t TimeFormat::toString(const time_t &time, const char *formatter, String &str) {
   const String s = asString(time, formatter);
   str.concat(s);
   return s.length();
}

/**
 * Formats the given local time as a string with the default time format - see \ref defaultTimePattern
 * @param time time to format - local seconds since 1/1/1970
 * @return time formatted string
 */
String TimeFormat::asString(const time_t &time) {
   return asString(time, defaultTimePattern);
}

/**
 * Formats the local time in milliseconds as a string with the default time format - see \ref defaultTimeMsPattern
 * @param timeMs time to format - local milliseconds since 1/1/1970
 * @return time formatted string including ms
 */
String TimeFormat::asStringMs(const time_t &timeMs) {
   const time_t time = timeMs / 1000;
   const int ms = static_cast<int>(timeMs % 1000);
   String str;
   str.reserve(TIME_BUFFER_LENGTH);
   char buf[TIME_BUFFER_LENGTH]{};
   tmElements_t tm;
   timeService.breakTime(time, tm);
   const int ofsHour = tm.tm_offset / 3600;
   const int ofsMin = (abs(tm.tm_offset) % 3600) / 60;
   snprintf(buf, TIME_BUFFER_LENGTH, defaultTimeMsPattern, tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ms, ofsHour, ofsMin, tm.tm_zone);
   str.concat(buf);
   return str;
}

/**
 * Formats the local time given as a string with the format provided
 * @param time time to format - local seconds since 1/1/1970
 * @param formatter format pattern - see https://en.cppreference.com/w/c/chrono/strftime
 * @return time formatted string
 */
String TimeFormat::asString(const time_t &time, const char *formatter) {
   String str;
   str.reserve(TIME_BUFFER_LENGTH);
   char buf[TIME_BUFFER_LENGTH]{};
   tmElements_t tm;
   timeService.breakTime(time, tm);
   strftime(buf, TIME_BUFFER_LENGTH-1, formatter, &tm);
   str.concat(buf);
   return str;
}
