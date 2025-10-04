//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved.
//
#include <Arduino.h>
#include "timeutil.h"
#include "PaletteFactory.h"
#include "util.h"
#include "sysinfo.h"
#include "constants.hpp"
#include "diag.h"
#include "log.h"

constexpr auto fmtDate PROGMEM = "%4d-%02d-%02d";
constexpr auto fmtTime PROGMEM = "%02d:%02d:%02d";
constexpr size_t TIME_BUFFER_SIZE = 32;
constexpr auto TIME_ZONE_NAME = "America/Chicago";
constexpr TimeChangeRule cdt {.name = "CDT", .week = Second, .dow = Sun, .month = Mar, .hour = 2, .offsetMin = -300};
constexpr TimeChangeRule cst {.name = "CST", .week = First, .dow = Sun, .month = Nov, .hour = 2, .offsetMin = -360};
Timezone centralTime(cdt, cst, "America/Chicago");

WiFiUDP* ntpUDP = nullptr;
TimeService timeService;

void updateLoggingTimebase() {
#if LOGGING_ENABLED == 1
    const time_t currentTime = nowMillis();
    const time_t currentMillis = millis();
    // if the log is off by more than 5 minutes, adjust it
    if (const time_t timeLog = currentMillis + Log.getTimebase(); abs(currentTime - timeLog) > 5 * SECS_PER_MIN * 1000 ) {
        const time_t logTimebase = Log.getTimebase();
        log_info(F("Logging time reference updated at RTC %lld ms from %s to %s"),
                currentMillis, TimeFormat::asStringMs(timeLog, false).c_str(), TimeFormat::asStringMs(currentTime).c_str());
        Log.setTimebase(currentTime - currentMillis);
        log_info(F("Logging timebase offset updated from %lld ms to %lld ms"), logTimebase, Log.getTimebase());
    } else
        log_info(F("Logging timebase offset not updated - log time %s is within 5 minutes of current time %s"),
            TimeFormat::asStringMs(timeLog, false).c_str(), TimeFormat::asStringMs(currentTime).c_str());
#endif
}

/**
 * Log current time information - successful NTP sync
 * @param curTime current time
 * @param holiday current holiday
 */
void logTimeStatus(const time_t curTime, const Holiday& holiday) {
#if LOGGING_ENABLED == 1
    const bool isDaylightSavings = timeService.timezone()->isDST(curTime);
    const char* const savingsType = isDaylightSavings ? "Daylight Savings" : "Standard";
    const int offset = timeService.timezone()->getOffset(curTime);

    const String strTime = TimeFormat::asString(curTime);
    
    log_info(F("%s %s time, time offset set to %d s, current time %s. NTP sync %s."), timeService.timezone()->getName(), savingsType,
        offset, strTime.c_str(), sysInfo->isSysStatus(SYS_STATUS_NTP) ? "ok" : "failed (fallback to other source)");
    log_info(F("Current time %s (holiday adjusted to %s); system status %#hX"), strTime.c_str(), holidayToString(holiday), sysInfo->getSysStatus());
    log_info(F("Time Sync: local millis RTC %lld to unix millis %lld"), timeService.syncLocalTimeMillis(), timeService.syncUTCTimeMillis());
#endif
}

/**
 * NTP sync was successful and time was within valid range; sets the NTP and DST flags accordingly.
 * Adjusts the current holiday based on time. Adjusts the logging timebase.
 * @return true (all the time)
 */
bool handleNTPSuccess() {
    const time_t curTime = now();
    const bool isDaylightSavings = timeService.timezone()->isDST(curTime);
    sysInfo->setSysStatus(SYS_STATUS_NTP);
    
    if (isDaylightSavings)
        sysInfo->setSysStatus(SYS_STATUS_DST);
    else
        sysInfo->resetSysStatus(SYS_STATUS_DST);

    const Holiday holiday = paletteFactory.adjustHoliday(curTime);
    updateLoggingTimebase();
    logTimeStatus(curTime, holiday);

    const TimeSync tSync {.localMillis = static_cast<ulong>(timeService.syncLocalTimeMillis()), .unixMillis=timeService.syncUTCTimeMillis() };
    timeSyncs.push(tSync);

    //update places where time has been captured before NTP sync - watchdog reboots
    for (auto &wdTime : sysInfo->watchdogReboots()) {
        if (wdTime < TWENTY_TWENTY)
            wdTime = timeService.utcFromRtcMillis(wdTime*1000)/1000;   //watchdog time is in seconds local; we're calling utc flavor as the time is already adjusted for local
    }
    //update the timestamps of temp calibration structures - those time values, if captured (through now()) are already adjusted for local timezone, hence converting them
    //to proper times is done using utcXYZ API to avoid double timezone offset adjustments
    if (calibCpuTemp.time > 0 && calibCpuTemp.time < TWENTY_TWENTY)
        calibCpuTemp.time = timeService.utcFromRtcMillis(calibCpuTemp.time * 1000)/1000;
    if (calibTempMeasurements.ref.time > 0 && calibTempMeasurements.ref.time < TWENTY_TWENTY)
        calibTempMeasurements.ref.time = timeService.utcFromRtcMillis(calibTempMeasurements.ref.time * 1000)/1000;
    if (calibTempMeasurements.max.time > 0 && calibTempMeasurements.max.time < TWENTY_TWENTY)
        calibTempMeasurements.max.time = timeService.utcFromRtcMillis(calibTempMeasurements.max.time * 1000)/1000;
    if (calibTempMeasurements.min.time > 0 && calibTempMeasurements.min.time < TWENTY_TWENTY)
        calibTempMeasurements.min.time = timeService.utcFromRtcMillis(calibTempMeasurements.min.time * 1000)/1000;

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

    if (const time_t wifiTime = WiFi.getTime(); wifiTime > 0) {
        timeService.setTime(wifiTime);
        const time_t curTime = now();
        const Holiday holiday = paletteFactory.adjustHoliday(curTime);
        updateLoggingTimebase();
        const bool isDaylightSavings = timeService.timezone()->isDST(wifiTime, false);
        if (isDaylightSavings)
            sysInfo->setSysStatus(SYS_STATUS_DST);
        log_warn(F("No NTP; Current time sourced from WiFi: %s %s (holiday adjusted to %s)"), TimeFormat::asString(curTime).c_str(),
            isDaylightSavings ? timeService.timezone()->getDSTShort() : timeService.timezone()->getSTDShort(), holidayToString(holiday));
        logTimeStatus(curTime, holiday);
    } else {
        log_error(F("No NTP, WiFi time not available. Current time from raw clock: %s (holiday adjusted to %s)"),
                  TimeFormat::asString(now()).c_str(), holidayToString(paletteFactory.getHoliday()));
    }
    
    log_info(F("Current holiday is %s; system status %#hX"), holidayToString(paletteFactory.getHoliday()), sysInfo->getSysStatus());
}

/**
 * Sets up the time callback and attempts to source current time from NTP.
 * Callers are ensuring this is called after WiFi connectivity is successful - otherwise there is no point in attempting this
 * as the meaningful fallback if on WiFi time. The last resort is the local unsynchronized time, which has no value.
 * @return true if the NTP sync was successful
 */
bool timeSetup() {
    timeBegin();
    timeService.applyTimezone(centralTime);
    if (sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        // if we have time already acquired through WiFi, use it - WiFi module got it from NTP as well
        bool ntpTimeAvailable = false;
        if (const time_t wifiTime = WiFi.getTime(); wifiTime > 0) {
            timeService.setTime(wifiTime);
            ntpTimeAvailable = true;
            log_info(F("Time service seeded from WiFi time (NTP based): %s"), TimeFormat::asString(wifiTime).c_str());
        } else {
            ntpTimeAvailable = timeService.syncTimeNTP();
            if (ntpTimeAvailable)
                log_info(F("Time service seeded from NTP pool: %s"), TimeFormat::asStringMs(timeService.syncUTCTimeMillis()).c_str());
            else
                log_warn(F("Acquiring NTP time has FAILED, retrying later..."));
        }

        if (ntpTimeAvailable)
            return handleNTPSuccess();
    } else {
        log_warn(F("WiFi connectivity not available - this call was made in error! (attempting to fall back to local time)"));
    }
    handleNTPFailure();
    return false;
}

/**
 * Ensures the UDP client is properly instantiated for the timeService - if a new instance is made, timeService.begin() is also called
 * Ensure this is called while WiFi is present - creating a WiFiUDP without WiFi connectivity may lead to unexpected behaviors
 */
void timeBegin() {
    //this requires WiFi!
    if (ntpUDP == nullptr) {
        ntpUDP = new WiFiUDP();
        timeService.begin(ntpUDP);
        log_info(F("NTP UDP client created"));
    }
}

/**
 * Are we in DST (Daylight Savings Time) at this time?
 * @param time time to check (unix time - seconds since 01/01/1970 adjusted for timezone)
 * @return true if time is in DST, false otherwise
 */
bool isDST(const time_t time) {
    return timeService.timezone()->isDST(time);
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
 * Encodes month (1-12; 0x01-0x0C) and day (1-31, 0x01-0x1F) (in this order) into a short unsigned int (2 bytes) such that it can be easily used
 * for comparisons
 * @param time (optional) specific time to encode for. If not specified, current time is used.
 * @return 2 byte encoded month and day
 */
uint16_t encodeMonthDay(const time_t time) {
    const time_t theTime = time == 0 ? now() : time;
    tmElements_t tm{};
    CoreTimeCalc::breakTimeCore(theTime, tm);
    return (((tm.tm_mon+1) & 0xFF) << 8) + (tm.tm_mday & 0xFF);
}

/**
 * Computes drift of local time from official time between two time sync points
 * @param from time sync point
 * @param to later time sync point
 * @return time drift in ms - positive means local time is faster, negative means local time is slower than the official time
 */
int getDrift(const TimeSync &from, const TimeSync &to) {
    const time_t localDelta = static_cast<time_t>(to.localMillis) - static_cast<time_t>(from.localMillis);
    const time_t unixDelta = to.unixMillis - from.unixMillis;
    return static_cast<int>(localDelta - unixDelta);
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
    const time_t start = timeSyncs.begin()->unixMillis;
    const time_t end = timeSyncs.end()[-1].unixMillis;       // end() is past the last element, -1 for the last element
    return static_cast<int>(getTotalDrift() * 3600000L / static_cast<long>(end - start));
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