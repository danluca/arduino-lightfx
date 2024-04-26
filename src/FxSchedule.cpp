// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include "FxSchedule.h"
#include "FastLED.h"
#include "global.h"
#include "timeutil.h"
#include "log.h"

const uint16_t dailyBedTime = 30*SECS_PER_MIN;          //12:30am bedtime
const uint16_t dailyWakeupTime = 6*SECS_PER_HOUR;       //6:00am wakeup time
const char strWakeup[] PROGMEM = "Wake-Up";
const char strBedtime[] PROGMEM = "Bed time";

uint16_t currentDay = 0;

std::deque<AlarmData*> scheduledAlarms;

const char *alarmTypeToString(AlarmType alType) {
    switch (alType) {
        case WAKEUP: return strWakeup;
        case BEDTIME: return strBedtime;
        default: return strNR;
    }
}

/**
 * Counts scheduled alarms of a particular type that haven't yet triggered (are in the future with respect to given time reference)
 * @param alType alarm type
 * @param refTime time reference
 * @return how many scheduled alarms fit the criteria above
 */
uint countFutureAlarms(const AlarmType alType, const time_t refTime) {
    uint count = 0;
    for (const auto &al : scheduledAlarms) {
        if (al->value > refTime && al->type == alType)
            count++;
    }
    return count;
}

/**
 * Sets the schedule for the day - ensures there are at least one alarm of each type scheduled in the future for reference time provided
 * @param time reference time
 */
void scheduleDay(const time_t time) {
    time_t startDay = previousMidnight(time);

    uint8_t curAlarmCount = scheduledAlarms.size();
    uint alarmCount = countFutureAlarms(WAKEUP, time);
    if (alarmCount < 1) {
        time_t upOn = startDay + dailyWakeupTime;
        //add wake-up alarm for today (if we have not passed it) or tomorrow (if we did)
        if (time < upOn)
            scheduledAlarms.push_back(new AlarmData{.value=upOn, .type=WAKEUP, .onEventHandler=wakeup});
        else
            scheduledAlarms.push_back(new AlarmData{.value=upOn + SECS_PER_DAY, .type=WAKEUP, .onEventHandler=wakeup});
    }
    alarmCount = countFutureAlarms(BEDTIME, time);
    if (alarmCount < 1) {
        //add sleep alarm for current day if not passed it, or next day; note the bedtime for today is set to past midnight for the next day
        time_t bedTime = startDay + dailyBedTime;
        if (time < bedTime)
            scheduledAlarms.push_back(new AlarmData{.value=bedTime, .type=BEDTIME, .onEventHandler=bedtime});
        else
            scheduledAlarms.push_back(new AlarmData{.value=bedTime + SECS_PER_DAY, .type=BEDTIME, .onEventHandler=bedtime});
    }
    Log.infoln(F("Scheduled %d new alarms for Day %y"), scheduledAlarms.size() - curAlarmCount, time);
}

/**
 * Logs the alarms to the console - info level
 */
void logAlarms() {
    for (const auto &al : scheduledAlarms)
        Log.infoln(F("Alarm %X type %d scheduled for %y; handler %X"), (long)al, al->type, al->value, (long)al->onEventHandler);
}

/**
 * Setup the default sleep/wake-up schedule
 */
void setupAlarmSchedule() {
    //alarms for today
    time_t time = now();
    currentDay = day(time);
    scheduleDay(time);
    adjustCurrentEffect(time);
    logAlarms();
}

bool isAwakeTime(time_t time) {
    time_t startDay = previousMidnight(time);
    time_t bedTime = startDay + dailyBedTime;
    time_t wakeTime = startDay + dailyWakeupTime;
    //if bedtime - midnight - waketime; then check if current time is before bedtime and after waketime
    if (bedTime > wakeTime)
        return !(time >= bedTime || time < wakeTime);
    //midnight - bedtime - waketime; check if current time is less than bedtime or more than waketime
    return (time >= wakeTime || time < bedTime);
}

void alarm_loop() {
    EVERY_N_SECONDS(60) {
        if (currentDay != day())
            setupAlarmSchedule();
        time_t time = now();
        for (auto it = scheduledAlarms.begin(); it != scheduledAlarms.end();) {
            auto al = *it;
            if (al->value <= time) {
                Log.infoln(F("Alarm %X type %d triggered at %y for scheduled time %y; handler %X"), (long)al, al->type, time, al->value, (long)al->onEventHandler);
                al->onEventHandler();
                it = scheduledAlarms.erase(it);
                delete al;
            } else
                ++it;
        }
        Log.infoln(F("Alarms remaining:"));
        logAlarms();

//        for (size_t x = 0; x < scheduledAlarms.size(); x++) {
//            AlarmData *al = scheduledAlarms.front();
//            scheduledAlarms.pop_front();
//            if (al->value <= time) {
//                Log.infoln("Alarm %X type %d triggered at %y for scheduled time %y; handler %X", (long)al, al->type, time, al->value, (long)al->onEventHandler);
//                al->onEventHandler();
//                delete al;
//            } else
//                scheduledAlarms.push_back(al);
//        }
    }
}
