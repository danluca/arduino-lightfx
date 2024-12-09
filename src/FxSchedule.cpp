// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include "FxSchedule.h"
#include "FastLED.h"
#include <FreeRTOS.h>
#include <queue.h>
#include <timers.h>
#include "global.h"
#include "sysinfo.h"
#include "util.h"

const uint16_t dailyBedTime = 30*SECS_PER_MIN;          //12:30am bedtime
const uint16_t dailyWakeupTime = 6*SECS_PER_HOUR;       //6:00am wakeup time
constexpr char strWakeup[] PROGMEM = "Wake-Up";
constexpr char strBedtime[] PROGMEM = "Bed-time";
static uint16_t tmrAlarmCheck = 30;

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
 * Finds the next alarm to trigger
 */
AlarmData *findNextAlarm() {
    AlarmData *nextAlarm = nullptr;
    for (const auto &al : scheduledAlarms) {
        if (!nextAlarm || al->value < nextAlarm->value)
            nextAlarm = al;
    }
    return nextAlarm;
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
 * Counts the alarms of a type that have been scheduled on the same day as the reference time
 * @param alType alarm type
 * @param refTime time reference
 * @return how many alarms are scheduled for same day as the reference time
 */
uint countTodayAlarms(const AlarmType alType, const time_t refTime) {
    time_t startDay = previousMidnight(refTime);
    time_t startNextDay = startDay+SECS_PER_DAY;
    uint count = 0;
    for (const auto &al : scheduledAlarms) {
        if (al->type == alType && al->value >= startDay && al->value < startNextDay)
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
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        Log.warningln(F("Cannot setup alarms without WiFi, likely time is not set"));
        return;
    }
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

/**
 * Enqueues an alarm check message to the core0 queue
 * @param xTimer the timer that triggered the alarm check
 */
void enqueueAlarmCheck(TimerHandle_t xTimer) {
    constexpr MiscAction action = ALARM_CHECK;
    if (BaseType_t qResult = xQueueSend(core0Queue, &action, pdMS_TO_TICKS(2000)); qResult != pdTRUE)
        Log.errorln(F("Error sending ALARM_CHECK message to core0 queue for timer %d [%s] - error %d"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
}

void alarm_setup() {
    setupAlarmSchedule();
    //at this point we should have a next alarm
    time_t nextAlarmCheck = 15*60; //default 15 minutes
    if (const AlarmData *nextAlarm = findNextAlarm())
        nextAlarmCheck = max((nextAlarm->value-now())/10, 60);  // set timer at minimum 1 minute or 10% of time to next alarm
    else
        Log.errorln(F("There are no alarms scheduled - checking alarms in 15 min, by default"));
    const TimerHandle_t thAlarmCheck = xTimerCreate("alarmCheck", pdMS_TO_TICKS(nextAlarmCheck*1000), pdFALSE, &tmrAlarmCheck, enqueueAlarmCheck);
    if (thAlarmCheck == nullptr)
        Log.errorln(F("Cannot create alarmCheck timer - Ignored. There is NO alarm check scheduled"));
    else if (xTimerStart(thAlarmCheck, 0) != pdPASS)
        Log.errorln(F("Cannot start the alarmCheck timer - Ignored."));

}

void alarm_check() {
    const time_t time = now();
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
    //if no more alarms or a new day - attempt to schedule next alarms
    if (scheduledAlarms.empty() || currentDay != day(time)) {
        Log.infoln(F("Alarms queue empty or a new day - scheduling more"));
        setupAlarmSchedule();
    } else {
        Log.infoln(F("Alarms remaining:"));
        logAlarms();
    }
    //at this point we should have a next alarm
    time_t nextAlarmCheck = 15*60; //default 15 minutes
    if (const AlarmData *nextAlarm = findNextAlarm())
        nextAlarmCheck = max((nextAlarm->value - now()) / 10, 60); // set timer at minimum 1 minute or 10% of time to next alarm
    else
        Log.errorln(F("There are no alarms scheduled - checking alarms in 15 min, by default"));
    const TimerHandle_t thAlarmCheck = xTimerCreate("alarmCheck", pdMS_TO_TICKS(nextAlarmCheck*1000), pdFALSE, &tmrAlarmCheck, enqueueAlarmCheck);
    if (thAlarmCheck == nullptr)
        Log.errorln(F("Cannot create alarmCheck timer - Ignored. There is NO alarm check scheduled"));
    else if (xTimerStart(thAlarmCheck, 0) != pdPASS)
        Log.errorln(F("Cannot start the alarmCheck timer - Ignored."));


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
