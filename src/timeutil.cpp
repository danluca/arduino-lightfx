// Copyright (c) 2023 by Dan Luca. All rights reserved.
//

#include "timeutil.h"
#include "PaletteFactory.h"

const char fmtDate[] PROGMEM = "%4d-%02d-%02d";
const char fmtTime[] PROGMEM = "%02d:%02d:%02d";

NTPClient timeClient(Udp, CST_OFFSET_SECONDS);  //time client, retrieves time from pool.ntp.org for CST

bool time_setup() {
    //read the time
    bool ntpTimeAvailable = ntp_sync();
    setSyncProvider(curUnixTime);
#ifndef DISABLE_LOGGING
    Log.warningln(F("Acquiring NTP time, attempt %s"), ntpTimeAvailable ? "was successful" : "has FAILED, retrying later...");
#endif
    Holiday hday;
    if (ntpTimeAvailable) {
        bool bDST = isDST(timeClient.getEpochTime());
        setSysStatus(SYS_STATUS_NTP);
        if (bDST) {
            setSysStatus(SYS_STATUS_DST);
            timeClient.setTimeOffset(CDT_OFFSET_SECONDS);   //getEpochTime calls account for the offset
        } else
            timeClient.setTimeOffset(CST_OFFSET_SECONDS);   //getEpochTime calls account for the offset
        setTime(timeClient.getEpochTime());    //ensure the offset change above (if it just transitioned) has taken effect
        hday = paletteFactory.adjustHoliday();    //update the holiday for new time
#ifndef DISABLE_LOGGING
        Log.infoln(F("America/Chicago %s time, time offset set to %d s, current time %s. NTP sync ok."),
                   bDST?"Daylight Savings":"Standard", bDST?CDT_OFFSET_SECONDS:CST_OFFSET_SECONDS, timeClient.getFormattedTime().c_str());
        char timeBuf[20];
        formatDateTime(timeBuf, now());
        Log.infoln(F("Current time %s %s (holiday adjusted to %s"), timeBuf, bDST?"CDT":"CST", holidayToString(hday));
#endif
    } else {
        resetSysStatus(SYS_STATUS_NTP);
        hday = paletteFactory.adjustHoliday();    //update the holiday for new time
        bool bDST = isDST(WiFi.getTime() + CST_OFFSET_SECONDS);     //borrowed from curUnixTime() - that is how DST flag is determined
        if (bDST)
            setSysStatus(SYS_STATUS_DST);
#ifndef DISABLE_LOGGING
        char timeBuf[20];
        formatDateTime(timeBuf, now());
        Log.warningln(F("NTP sync failed. Current time sourced from WiFi: %s %s (holiday adjusted to %s)"),
              timeBuf, bDST?"CDT":"CST", holidayToString(hday));
    }
    Log.infoln(F("Current holiday is %s"), holidayToString(hday));
#else
    }
#endif
    return ntpTimeAvailable;
}

/**
 * Formats the time component of the timestamp, using a standard pattern - @see #fmtTime
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
 * Formats the date component of the timestamp, using a standard pattern - @see #fmtDate
 * @param buf buffer to write to. If not null, it must have space for 11 characters
 * @param time the time to format, if not specified defaults to @see now()
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
    if (isSysStatus(SYS_STATUS_WIFI)) {
        //the WiFi.getTime() (returns unsigned long, 0 for failure) can also achieve time telling purpose
        //determine what offset to use
        time_t wifiTime = WiFi.getTime() + CST_OFFSET_SECONDS;
        if (isDST(wifiTime))
            wifiTime = WiFi.getTime() + CDT_OFFSET_SECONDS;
        return wifiTime;
    }
    return 0;
}

bool ntp_sync() {
    timeClient.begin();
    timeClient.update();
    timeClient.end();
    return timeClient.isTimeSet();
}

/**
 * Figures out the holiday type from a given time - based on month and day elements
 * @param time
 * @return one of the pre-defined holidays, defaults to Party
 */
Holiday buildHoliday(time_t time) {
    const uint16_t md = encodeMonthDay(time);
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
    return isSysStatus(SYS_STATUS_WIFI) ? buildHoliday(now()) : Party;
}

/**
 * Parse a string into a Holiday value - coded the hard way, supposedly a lookup map would be more convenient, but likely take more memory...
 * @param str
 * @return
 */
Holiday parseHoliday(const String *str) {
    if (str->equalsIgnoreCase("Halloween"))
        return Halloween;
    else if (str->equalsIgnoreCase("Thanksgiving"))
        return Thanksgiving;
    else if (str->equalsIgnoreCase("Christmas"))
        return Christmas;
    else if (str->equalsIgnoreCase("NewYear"))
        return NewYear;
    return Party;
}

const char *holidayToString(const Holiday hday) {
    switch (hday) {
        case None:
            return "None";
        case Party:
            return "Party";
        case Halloween:
            return "Halloween";
        case Thanksgiving:
            return "Thanksgiving";
        case Christmas:
            return "Christmas";
        case NewYear:
            return "NewYear";
        default:
            return "N/R";
    }
}

/**
 * Encodes month and day (in this order) into a short unsigned int (2 bytes) such that it can be easily used
 * for comparisons
 * @param time (optional) specific time to encode for. If not specified, current time is used.
 * @return 2 byte encoded month and day
 */
uint16_t encodeMonthDay(const time_t time) {
    time_t theTime = time == 0 ? now() : time;
    return ((month(theTime) & 0xFF) << 8) + (day(theTime) & 0xFF);
}

