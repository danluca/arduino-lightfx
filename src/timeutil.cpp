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

WiFiUDP Udp;  // A UDP instance to let us send and receive packets over UDP
NTPClient timeClient(Udp, CST_OFFSET_SECONDS);  //time client, retrieves time from pool.ntp.org for CST

bool timeSetup() {
    //read the time
    setSyncProvider(curUnixTime);
    const bool ntpTimeAvailable = ntp_sync();
    log_warn(F("Acquiring NTP time, attempt %s"), ntpTimeAvailable ? "was successful" : "has FAILED, retrying later...");
    Holiday hday;
    if (ntpTimeAvailable) {
        const bool bDST = isDST(timeClient.getEpochTime());
        sysInfo->setSysStatus(SYS_STATUS_NTP);
        if (bDST) {
            sysInfo->setSysStatus(SYS_STATUS_DST);
            timeClient.setTimeOffset(CDT_OFFSET_SECONDS);   //getEpochTime calls account for the offset
        } else
            timeClient.setTimeOffset(CST_OFFSET_SECONDS);   //getEpochTime calls account for the offset
        setTime(timeClient.getEpochTime());    //ensure the offset change above (if it just transitioned) has taken effect
        hday = paletteFactory.adjustHoliday();    //update the holiday for new time
#if LOGGING_ENABLED == 1
        if (Log.getTimebase() == 0) {
            const time_t curTime = now();
            const time_t curMs = millis();
            log_warn(F("Logging time reference updated from %llu ms (%s) to %s"), curMs, StringUtils::asString(curMs/1000).c_str(), StringUtils::asString(curTime).c_str());
            Log.setTimebase(curTime * 1000 - curMs);                //capture current time into the log offset, such that log statements use current time
        }
#endif
        log_info(F("America/Chicago %s time, time offset set to %d s, current time %s. NTP sync ok."),
                   bDST?"Daylight Savings":"Standard", bDST?CDT_OFFSET_SECONDS:CST_OFFSET_SECONDS, timeClient.getFormattedTime().c_str());
        char timeBuf[20];
        formatDateTime(timeBuf, now());
        log_info(F("Current time %s %s (holiday adjusted to %s"), timeBuf, bDST?"CDT":"CST", holidayToString(hday));
    } else {
        sysInfo->resetSysStatus(SYS_STATUS_NTP);
        paletteFactory.setHoliday(Party);  //setting it explicitly to avoid defaulting to none when there is no wifi altogether
        hday = paletteFactory.adjustHoliday();    //update the holiday for new time
        const bool bDST = isDST(WiFi.getTime() + CST_OFFSET_SECONDS);     //borrowed from curUnixTime() - that is how DST flag is determined
        if (bDST)
            sysInfo->setSysStatus(SYS_STATUS_DST);
        char timeBuf[20];
        formatDateTime(timeBuf, now());
        log_warn(F("NTP sync failed. Current time sourced from WiFi: %s %s (holiday adjusted to %s)"),
              timeBuf, bDST?"CDT":"CST", holidayToString(hday));
    }
    log_info(F("Current holiday is %s"), holidayToString(hday));
    return ntpTimeAvailable;
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
    if (timeClient.isTimeSet())
        return timeClient.getEpochTime();
    if (sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        //the WiFi.getTime() (returns unsigned long, 0 for failure) can also achieve time telling purpose
        //determine what offset to use
        const time_t wifiTime = WiFi.getTime();
        time_t localTime = wifiTime + CST_OFFSET_SECONDS;
        if (isDST(localTime))
            localTime = wifiTime + CDT_OFFSET_SECONDS;
        return localTime;
    }
    return millis();
}

bool ntp_sync() {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        log_warn(F("NTP sync failed. No WiFi connection available."));
        return false;
    }
    timeClient.begin();
    timeClient.update();
    timeClient.end();
    const bool result = timeClient.isTimeSet();
    if (result) {
        const TimeSync tSync {.localMillis = millis(), .unixSeconds=now()};
        timeSyncs.push(tSync);
    }
    return result;
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

