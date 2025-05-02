//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved.
//
#include <Arduino.h>
#include "timeutil.h"
#include "PaletteFactory.h"
#include "util.h"
#include "stringutils.h"
#include "sysinfo.h"
#include "constants.hpp"
#include "log.h"

constexpr auto fmtDate PROGMEM = "%4d-%02d-%02d";
constexpr auto fmtTime PROGMEM = "%02d:%02d:%02d";
constexpr size_t TIME_BUFFER_SIZE = 20;
constexpr auto TIME_ZONE_NAME = "America/Chicago";

WiFiUDP Udp;  // A UDP instance to let us send and receive packets over UDP
NTPClient timeClient(Udp, CST_OFFSET_SECONDS);  //time client, retrieves time from pool.ntp.org for CST


void updateLoggingTimebase() {
#if LOGGING_ENABLED == 1
    if (Log.getTimebase() == 0) {
        const time_t currentTime = now();
        const time_t currentMillis = millis();
        log_warn(F("Logging time reference updated from %llu ms (%s) to %s"), 
                currentMillis, StringUtils::asString(currentMillis/1000).c_str(), StringUtils::asString(currentTime).c_str());
        Log.setTimebase(currentTime * 1000 - currentMillis);
    }
#endif
}

/**
 * Adjusts America/Chicago time zone offset with DST
 * @param isDaylightSavings whether current time is in Daylight Savings period
 */
void configureTimeOffset(const bool isDaylightSavings) {
    const int timeOffset = isDaylightSavings ? CDT_OFFSET_SECONDS : CST_OFFSET_SECONDS;
    timeClient.setTimeOffset(timeOffset);
    setTime(timeClient.getEpochTime());
}

/**
 * Log current time information - successful NTP sync
 * @param curTime current time
 * @param holiday current holiday
 */
void logTimeStatus(const time_t curTime, const Holiday& holiday) {
#if LOGGING_ENABLED == 1
    const bool isDaylightSavings = isDST(curTime);
    const char* const timeZoneType = isDaylightSavings ? "CDT" : "CST";
    const char* const savingsType = isDaylightSavings ? "Daylight Savings" : "Standard";
    const int offset = isDaylightSavings ? CDT_OFFSET_SECONDS : CST_OFFSET_SECONDS;
    
    char timeBuffer[TIME_BUFFER_SIZE];
    formatDateTime(timeBuffer, curTime);
    
    log_info(F("%s %s time, time offset set to %d s, current time %s. NTP sync %s."), TIME_ZONE_NAME, savingsType,
        offset, timeClient.getFormattedTime().c_str(), sysInfo->isSysStatus(SYS_STATUS_NTP) ? "ok" : "failed (fallback to WiFi time)");
    log_info(F("Current time %s %s (holiday adjusted to %s); system status %#hX"), timeBuffer, timeZoneType, holidayToString(holiday), sysInfo->getSysStatus());
#endif
}

/**
 * NTP sync was successful and time was within valid range; sets the NTP and DST flags accordingly.
 * Adjusts the current holiday based on time. Adjusts the logging timebase.
 * @return true (all the time)
 */
bool handleNTPSuccess() {
    const bool isDaylightSavings = isDST(timeClient.getEpochTime());
    sysInfo->setSysStatus(SYS_STATUS_NTP);
    
    if (isDaylightSavings)
        sysInfo->setSysStatus(SYS_STATUS_DST);
    else
        sysInfo->resetSysStatus(SYS_STATUS_DST);

    configureTimeOffset(isDaylightSavings);
    const time_t curTime = now();
    const Holiday holiday = paletteFactory.adjustHoliday(curTime);
    updateLoggingTimebase();
    logTimeStatus(curTime, holiday);
    
    return true;
}

/**
 * Handles the case when NTP sync fails, either due to a timeout or a failure to get a valid time from the pool.ntp.org server
 * Attempts to leverage Wi-Fi time, potentially sourced from NTP as well. Adjusts the current holiday based on Wi-Fi time if available,
 * fallback to Party if not.
 */
void handleNTPFailure() {
    sysInfo->resetSysStatus(SYS_STATUS_NTP);
    paletteFactory.setHoliday(Party);

    const time_t wifiTime = WiFi.getTime();
    char timeBuffer[TIME_BUFFER_SIZE];
    Holiday holiday = paletteFactory.adjustHoliday(wifiTime);

    if (wifiTime > 0) {
        const bool isDaylightSavings = isDST(wifiTime);
        if (isDaylightSavings)
            sysInfo->setSysStatus(SYS_STATUS_DST);
        formatDateTime(timeBuffer, wifiTime);
        log_warn(F("NTP sync failed. Current time sourced from WiFi: %s %s (holiday adjusted to %s)"), timeBuffer,
            isDaylightSavings ? "CDT" : "CST", holidayToString(holiday));
    } else {
        formatDateTime(timeBuffer, now());
        log_error(F("NTP sync failed, WiFi time not available. Current time from raw clock: %s (holiday adjusted to %s)"), 
                  timeBuffer, holidayToString(holiday));
    }
    
    log_info(F("Current holiday is %s; system status %#hX"), holidayToString(holiday), sysInfo->getSysStatus());
}

/**
 * Sets up the time callback and attempts to source current time from NTP.
 * @return true if the NTP sync was successful
 */
bool timeSetup() {
    setSyncProvider(curUnixTime);
    const bool ntpTimeAvailable = ntp_sync();
    
    log_warn(F("Acquiring NTP time, attempt %s"), ntpTimeAvailable ? "was successful" : "has FAILED, retrying later...");
    
    if (ntpTimeAvailable)
        return handleNTPSuccess();

    handleNTPFailure();
    return false;
}

/**
 * Formats the time component of the timestamp, using a standard pattern - \see ::fmtTime
 * @param buf buffer to write to. If not null, it must have space for 9 characters
 * @param time the time to format, if not specified defaults to @see now()
 * @return number of characters written to the buffer for given time value
 */
uint8_t formatTime(char *buf, time_t time) {
    if (time == 0)
        time = now();
    if (buf == nullptr)
        return snprintf(buf, 0, fmtTime, hour(time), minute(time), second(time));
    return snprintf(buf, 9, fmtTime, hour(time), minute(time), second(time));   //8 chars + null terminating
}

/**
 * Formats the date component of the timestamp, using a standard pattern - \ref ::fmtDate
 * @param buf buffer to write to. If not null, it must have space for 11 characters
 * @param time the time to format, if not specified defaults to \code now()\endcode
 * @return number of characters written to the buffer for given time value
 */
uint8_t formatDate(char *buf, time_t time) {
    if (time == 0)
        time = now();
    if (buf == nullptr)
        return snprintf(buf, 0, fmtDate, year(time), month(time), day(time));
    return snprintf(buf, 11, fmtDate, year(time), month(time), day(time));   //10 chars + null terminating
}

uint8_t formatDateTime(char *buf, time_t time) {
    if (time == 0)
        time = now();
    uint8_t sz = formatDate(buf, time);
    if (buf == nullptr)
        sz += formatTime(buf, time);
    else {
        *(buf + sz) = ' ';  //date - time separation character
        sz += formatTime( buf+sz+1, time);
    }
    return sz;
}

time_t curUnixTime() {
    // we'll check our custom NTP flag for a more reliable test of a valid time - sometimes the NTP sync ends up with a wild time and timeClient.isTimeSet() reports true
    if (sysInfo->isSysStatus(SYS_STATUS_NTP))
        return timeClient.getEpochTime();
    if (sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        //the WiFi.getTime() (returns unsigned long, 0 for failure) can also achieve time-telling purpose
        //determine what offset to use
        if (const time_t wifiTime = WiFi.getTime(); wifiTime > 0) {
            time_t localTime = wifiTime + CST_OFFSET_SECONDS;
            if (isDST(localTime))
                localTime = wifiTime + CDT_OFFSET_SECONDS;
            return localTime;
        } else
            log_warn(F("WiFi.getTime() failed - returned 0, falling back to local millis()"));
    }
    return millis()/1000;    //nowhere close to reality, but better than nothing
}

#define TWENTY_TWENTY    1577836800L    //2020-01-01 00:00
#define TWENTY_SEVENTY   3155760000L    //2070-01-01 00:00 - if this code is still relevant in 2070, something is wrong...

bool ntp_sync() {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        log_warn(F("NTP sync failed. No WiFi connection available."));
        return false;
    }
    timeClient.begin();
    const bool result = timeClient.update();
    timeClient.end();
    const time_t curTime = now();
    const bool resultValid = result && curTime > TWENTY_TWENTY && curTime < TWENTY_SEVENTY;
    if (resultValid) {
        const TimeSync tSync {.localMillis = millis(), .unixSeconds=curTime};
        timeSyncs.push(tSync);
    } else {
        log_warn(F("NTP sync succeeded but time is invalid - outside the range %s - %s. Current time %s."), StringUtils::asString(static_cast<time_t>(TWENTY_TWENTY)).c_str(),
            StringUtils::asString(static_cast<time_t>(TWENTY_SEVENTY)).c_str(), StringUtils::asString(curTime).c_str());
    }
    return resultValid;
}

/**
 * Are we in DST (Daylight Savings Time) at this time?
 * @param time
 * @return
 */
bool isDST(const time_t time) {
//    const uint16_t md = encodeMonthDay(time);
    // switch the time offset for CDT between March 12th and Nov 5th - these are chosen arbitrary (matches 2023 dates) but close enough
    // to the transition, such that we don't need to implement complex Sunday counting rules
//    return md > 0x030C && md < 0x0B05;
    const int mo = month(time);
    const int dy = day(time);
    const int hr = hour(time);
    const int dow = weekday(time);
    // DST runs from second Sunday of March to first Sunday of November
    // Never in January, February or December
    if (mo < 3 || mo > 11)
        return false;
    // Always in April to October
    if (mo > 3 && mo < 11)
        return true;
    // In March, DST if previous Sunday was on or after the 8th.
    // Begins at 2am on second Sunday in March
    const int previousSunday = dy - dow;
    if (mo == 3)
        return previousSunday >= 7 && (!(previousSunday < 14 && dow == 1) || (hr >= 2));
    // Otherwise November, DST if before the first Sunday, i.e. the previous Sunday must be before the 1st
    return (previousSunday < 7 && dow == 1) ? (hr < 2) : (previousSunday < 0);
}

/**
 * Figures out the holiday type from a given time - based on month and day elements
 * @param time
 * @return one of the pre-defined holidays, defaults to Party
 */
Holiday buildHoliday(const time_t time) {
    const uint16_t md = encodeMonthDay(time);
    //Valentine's Day: Feb 11 through 15
    if (md > 0x020B && md < 0x0210)
        return ValentineDay;
    //StPatrick's Day: March 15 through 18
    if (md > 0x030E && md < 0x0313)
        return StPatrick;
    //Memorial Day: May 25 through May 31
    if (md > 0x0518 && md < 0x0600)
        return MemorialDay;
    //Independence Day: Jul 1 through Jul 5
    if (md > 0x0700 && md < 0x0706)
        return IndependenceDay;
    //Halloween: Oct 1 through Nov 3
    if (md > 0xA00 && md < 0xB04)
        return Halloween;
    //Thanksgiving: Nov 04 through Nov 30
    if (md > 0xB03 && md < 0xB1F)
        return Thanksgiving;
    //Christmas: Dec 23 through Dec 27
    if (md > 0xC16 && md < 0xC1C)
        return Christmas;
    //NewYear: Dec 30 through Jan 2
    if (md > 0xC1E || md < 0x103)
        return NewYear;
    //Party: all others (winter holidays)
    return Party;
}

Holiday currentHoliday() {
    return sysInfo->isSysStatus(SYS_STATUS_WIFI) ? buildHoliday(now()) : Party;
}

/**
 * Parse a string into a Holiday value - coded the hard way, supposedly a lookup map would be more convenient, but likely take more memory...
 * @param str
 * @return
 */
Holiday parseHoliday(const String *str) {
    if (str->equalsIgnoreCase(strValentine))
        return ValentineDay;
    if (str->equalsIgnoreCase(strStPatrick))
        return StPatrick;
    if (str->equalsIgnoreCase(strMemorialDay))
        return MemorialDay;
    if (str->equalsIgnoreCase(strIndependenceDay))
        return IndependenceDay;
    if (str->equalsIgnoreCase(strHalloween))
        return Halloween;
    if (str->equalsIgnoreCase(strThanksgiving))
        return Thanksgiving;
    if (str->equalsIgnoreCase(strChristmas))
        return Christmas;
    if (str->equalsIgnoreCase(strNewYear))
        return NewYear;
    if (str->equalsIgnoreCase(strNone))
        return None;
    return Party;
}

const char *holidayToString(const Holiday hday) {
    switch (hday) {
        case None:
            return strNone;
        case Party:
            return strParty ;
        case ValentineDay:
            return strValentine;
        case StPatrick:
            return strStPatrick;
        case MemorialDay:
            return strMemorialDay;
        case IndependenceDay:
            return strIndependenceDay;
        case Halloween:
            return strHalloween;
        case Thanksgiving:
            return strThanksgiving;
        case Christmas:
            return strChristmas;
        case NewYear:
            return strNewYear;
        default:
            return strNR;
    }
}

/**
 * Encodes month and day (in this order) into a short unsigned int (2 bytes) such that it can be easily used
 * for comparisons
 * @param time (optional) specific time to encode for. If not specified, current time is used.
 * @return 2 byte encoded month and day
 */
uint16_t encodeMonthDay(const time_t time) {
    const time_t theTime = time == 0 ? now() : time;
    return ((month(theTime) & 0xFF) << 8) + (day(theTime) & 0xFF);
}

/**
 * Computes drift of local time from official time between two time sync points
 * @param from time sync point
 * @param to later time sync point
 * @return time drift in ms - positive means local time is faster, negative means local time is slower than the official time
 */
int getDrift(const TimeSync &from, const TimeSync &to) {
    const time_t localDelta = (time_t)to.localMillis - (time_t)from.localMillis;
    const time_t unixDelta = (to.unixSeconds - from.unixSeconds)*1000l;
    return (int)(localDelta-unixDelta);
}

/**
 * Total amount of drift accumulated in the time sync points
 * @return total time drift in ms
 */
int getTotalDrift() {
    if (timeSyncs.size() < 2)
        return 0;
    int drift = 0;
    const TimeSync *prevSync = nullptr;
    for (auto &ts: timeSyncs) {
        if (prevSync == nullptr)
            prevSync = &ts;
        else
            drift += getDrift(*prevSync, ts);
    }
    return drift;
}

/**
 * Total time drift averaged over the time period accumulated in the time sync points
 * @return average time drift in ms/hour
 */
int getAverageTimeDrift() {
    if (timeSyncs.size() < 2)
        return 0;
    const time_t start = timeSyncs.begin()->unixSeconds;
    const time_t end = timeSyncs.end()[-1].unixSeconds;       // end() is past the last element, -1 for last element
    return getTotalDrift() * 3600 / (int)(end-start);
}

/**
 * Most recently measured time drift (between last two time sync points)
 * @return most recent measured time drift in ms
 */
int getLastTimeDrift() {
    if (timeSyncs.size() < 2)
        return 0;
    const TimeSync &lastSync = timeSyncs.back();
    const TimeSync &prevSync = timeSyncs.end()[-2];   // end() is past the last element, -1 for last element, -2 for second-last
    return getDrift(prevSync, lastSync);
}