// Copyright (c) by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_FXSCHEDULE_H
#define ARDUINO_LIGHTFX_FXSCHEDULE_H

#include "Arduino.h"
#include <vector>

void alarm_setup();
void alarm_check();
void setupAlarmSchedule();

void wakeup();
void bedtime();
void adjustCurrentEffect(time_t time);
bool isAwakeTime(time_t time);

typedef void (*AlarmHandlerPtr)();  // alarm callback function typedef

enum AlarmType:uint8_t {
    BEDTIME, WAKEUP
};

const char* alarmTypeToString(AlarmType alType);
uint countUpcomingAlarms(AlarmType alType, time_t refTime);

struct AlarmData {
    const time_t value;
    const AlarmType type;
    AlarmHandlerPtr const onEventHandler;
};

// Returns a snapshot copy of scheduled alarms under lock - safe to use from any task
std::vector<AlarmData> getScheduledAlarmsCopy();

#endif //ARDUINO_LIGHTFX_FXSCHEDULE_H
