// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_FXSCHEDULE_H
#define ARDUINO_LIGHTFX_FXSCHEDULE_H

#include "Arduino.h"
#include <deque>

void alarm_setup();
void alarm_check();
void setupAlarmSchedule();

void wakeup();
void bedtime();
void adjustCurrentEffect(time_t time);
bool isAwakeTime(time_t time);

extern QueueHandle_t almQueue;

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
