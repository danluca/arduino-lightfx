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
    monthStr1,monthStr2,monthStr3,monthStr4,monthStr5,monthStr6,
    monthStr7,monthStr8,monthStr9,monthStr10,monthStr11,monthStr12
};

static inline constexpr char monthShortNames_P[] PROGMEM = "JanFebMarAprMayJunJulAugSepOctNovDec";

static inline constexpr char dayStr1[] PROGMEM = "Sunday";
static inline constexpr char dayStr2[] PROGMEM = "Monday";
static inline constexpr char dayStr3[] PROGMEM = "Tuesday";
static inline constexpr char dayStr4[] PROGMEM = "Wednesday";
static inline constexpr char dayStr5[] PROGMEM = "Thursday";
static inline constexpr char dayStr6[] PROGMEM = "Friday";
static inline constexpr char dayStr7[] PROGMEM = "Saturday";

static inline const PROGMEM constexpr char * const PROGMEM dayNames_P[] = {
   dayStr1,dayStr2,dayStr3,dayStr4,dayStr5,dayStr6,dayStr7
};

static inline constexpr char dayShortNames_P[] PROGMEM = "SunMonTueWedThuFriSat";
// static inline constexpr auto defaultTimePattern PROGMEM = "%Y-%m-%d %H:%M:%S %z %Z";
// static inline constexpr auto defaultTimePattern PROGMEM = "%F %T %z %Z (%c)";
static inline constexpr auto defaultDateTimePattern PROGMEM = "%d-%02d-%02d %02d:%02d:%02d %+03d:%02d %s";
static inline constexpr auto defaultDateTimePatternNoTZ PROGMEM = "%d-%02d-%02d %02d:%02d:%02d";
static inline constexpr auto defaultDateTimeMsPattern PROGMEM = "%d-%02d-%02d %02d:%02d:%02d.%03d %+03d:%02d %s";
static inline constexpr auto defaultDateTimeMsPatternNoTZ PROGMEM = "%d-%02d-%02d %02d:%02d:%02d.%03d";
static inline constexpr auto defaultDatePattern PROGMEM = "%d-%02d-%02d";
static inline constexpr auto defaultLocalTimePattern PROGMEM = "%02d:%02d:%02d";
static inline constexpr auto defaultOffsetPattern PROGMEM = "%+03d:%02d %s";

/**
 * Converts the numeric month value into its string representation - e.g. 1 -> February
 * NOTE: no null termination performed
 * @param month the month value, 0-11, January is 0
 * @param buffer buffer where to write the string representation
 * @return number of characters written
 */
size_t TimeFormat::monthStr(const uint8_t month, char *buffer) {
   PGM_P pgm_ptr = static_cast<const char *>(pgm_read_ptr(&(monthNames_P[month])));
   strcpy_P(buffer, pgm_ptr);
   return strlen_P(pgm_ptr);
}
/**
 * Converts the numeric month value into its short string representation - e.g. 1 -> Feb
 * NOTE: no null termination performed
 * @param month the month value, 0-11, January is 0
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
 * Converts the numeric day  value into its string representation - e.g. 1 -> Monday
 * NOTE: no null termination performed
 * @param day the day value, 0-6, Sunday is 0
 * @param buffer buffer where to write the string representation
 * @return number of characters written
 */
size_t TimeFormat::dayStr(const uint8_t day, char *buffer) {
   PGM_P pgm_ptr = static_cast<const char *>(pgm_read_ptr(&(dayNames_P[day])));
   strcpy_P(buffer, pgm_ptr);
   return strlen_P(pgm_ptr);
}

/**
 * Converts the numeric day value into its short string representation - e.g. 1 -> Mon
 * NOTE: no null termination performed
 * @param day the day value, 0-6, Sunday is 0
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
 * @param includeTZ whether to include the timezone information; default true
 * @return time formatted string
 */
String TimeFormat::asString(const time_t &time, const bool includeTZ) {
   return asString(time, defaultDateTimePattern, includeTZ);
}

/**
 * Formats the local time in milliseconds as a string with the default time format - see \ref defaultTimeMsPattern
 * @param timeMs time to format - local milliseconds since 1/1/1970
 * @param includeTZ whether to include the timezone information; default true
 * @return time formatted string including ms
 */
String TimeFormat::asStringMs(const time_t &timeMs, const bool includeTZ) {
   const time_t time = timeMs / 1000;
   const int ms = static_cast<int>(timeMs % 1000);
   String str;
   str.reserve(TIME_BUFFER_LENGTH);
   char buf[TIME_BUFFER_LENGTH]{};
   tmElements_t tm;
   timeService.breakTime(time, tm);
   if (includeTZ) {
      const int ofsHour = tm.tm_offset / 3600;
      const int ofsMin = (abs(tm.tm_offset) % 3600) / 60;
      snprintf(buf, TIME_BUFFER_LENGTH, defaultDateTimeMsPattern, tm.tm_year + TM_EPOCH_YEAR, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ms, ofsHour, ofsMin, tm.tm_zone);
   } else
      snprintf(buf, TIME_BUFFER_LENGTH, defaultDateTimeMsPatternNoTZ, tm.tm_year + TM_EPOCH_YEAR, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
   str.concat(buf);
   return str;
}

/**
 * Formats the local time given as a string with the format provided
 * @param time time to format - local seconds since 1/1/1970
 * @param formatter (temporarily ignored) format pattern - see https://en.cppreference.com/w/c/chrono/strftime
 * @param includeTZ whether to include the timezone information; default true
 * @return time formatted string
 */
String TimeFormat::asString(const time_t &time, const char *formatter, const bool includeTZ) {
   String str;
   str.reserve(TIME_BUFFER_LENGTH);
   char buf[TIME_BUFFER_LENGTH]{};
   tmElements_t tm;
   timeService.breakTime(time, tm);
   // strftime doesn't look to use tm_offset, tm_zone fields for the formatters %z, %Z respectively - the output we've seen is always '+0000 GMT'
   // strftime(buf, TIME_BUFFER_LENGTH-1, formatter, &tm);
   //forcing a default (ISO8601) formatter to ensure all fields passed as arguments are used in the formatter in proper order
   //client code that needs custom formatters need to perform their own time breaking and invoking snprintf with appropriate format and time field arguments
   if (includeTZ) {
      const int ofsHour = tm.tm_offset / 3600;
      const int ofsMin = (abs(tm.tm_offset) % 3600) / 60;
      snprintf(buf, TIME_BUFFER_LENGTH, defaultDateTimePattern, tm.tm_year + TM_EPOCH_YEAR, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ofsHour, ofsMin, tm.tm_zone);
   } else
      snprintf(buf, TIME_BUFFER_LENGTH, defaultDateTimePatternNoTZ, tm.tm_year + TM_EPOCH_YEAR, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
   str.concat(buf);
   return str;
}

/**
 * Converts the given time to its date string representation using the default formatter.
 * @param time the time value to be formatted
 * @return the date string representation of the given time
 */
String TimeFormat::dateAsString(const time_t &time) {
   String str;
   str.reserve(12);  //the date pattern is 10 chars long
   char buf[12]{};
   tmElements_t tm;
   timeService.breakTime(time, tm);
   snprintf(buf, 12, defaultDatePattern, tm.tm_year + TM_EPOCH_YEAR, tm.tm_mon+1, tm.tm_mday);
   str.concat(buf);
   return str;
}

/**
 * Converts a given time value into its string representation, optionally including the time zone.
 * The format of the returned string is standard time representation - 'HH:mm:ss'
 *
 * @param time the time value to be converted to a string representation
 * @param includeTZ flag indicating whether to include time zone information in the output
 * @return the string representation of the given time
 */
String TimeFormat::timeAsString(const time_t &time, const bool includeTZ) {
   String str;
   str.reserve(20);  //the time pattern is 8 chars long ('HH:mm:ss'), offset pattern is 10 ('+HH:mm XYZ') plus a separator space
   char buf[21]{};
   tmElements_t tm;
   timeService.breakTime(time, tm);
   const size_t sz = snprintf(buf, 9, defaultLocalTimePattern, tm.tm_hour, tm.tm_min, tm.tm_sec);
   if (includeTZ) {
      const int ofsHour = tm.tm_offset / 3600;
      const int ofsMin = (abs(tm.tm_offset) % 3600) / 60;
      buf[sz] = ' ';    //add a space between time and offset
      snprintf(buf+sz+1, 11, defaultOffsetPattern, ofsHour, ofsMin, tm.tm_zone);
   }
   str.concat(buf);
   return str;
}
