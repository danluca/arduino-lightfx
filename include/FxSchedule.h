// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_FXSCHEDULE_H
#define ARDUINO_LIGHTFX_FXSCHEDULE_H

#include <deque>
#include "Arduino.h"

void setupAlarmSchedule();
void alarm_loop();

void wakeup();
void bedtime();
void adjustCurrentEffect(time_t time);
bool isAwakeTime(time_t time);

typedef void (*AlarmHandlerPtr)();  // alarm callback function typedef

enum AlarmType:uint8_t {
    BEDTIME, WAKEUP
};

const char* alarmTypeToString(AlarmType alType);
uint countFutureAlarms(AlarmType alType, time_t refTime);

struct AlarmData {
    const time_t value;
    const AlarmType type;
    AlarmHandlerPtr const onEventHandler;
};

extern std::deque<AlarmData*> scheduledAlarms;

#endif //ARDUINO_LIGHTFX_FXSCHEDULE_H
