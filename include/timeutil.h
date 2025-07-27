//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_TIMEUTIL_H
#define ARDUINO_LIGHTFX_TIMEUTIL_H

#include <WiFiNINA.h>
#include <TimeLib.h>
#include "fixed_queue.h"

enum Holiday { None, Party, ValentineDay, StPatrick, MemorialDay, IndependenceDay, Halloween, Thanksgiving, Christmas, NewYear };
Holiday buildHoliday(time_t time);
Holiday currentHoliday();
Holiday parseHoliday(const String *str);
const char *holidayToString(Holiday hday);

uint8_t formatTime(char *buf, time_t time = 0);
uint8_t formatDate(char *buf, time_t time = 0);
uint8_t formatDateTime(char *buf, time_t time = 0);
bool isDST(time_t time);
uint16_t encodeMonthDay(time_t time = 0);

struct TimeSync {
    ulong localMillis{};
    time_t unixMillis{};
};

bool timeSetup();
int getAverageTimeDrift();
int getLastTimeDrift();
int getTotalDrift();
int getDrift(const TimeSync &from, const TimeSync &to);

extern FixedQueue<TimeSync, 8> timeSyncs;
extern WiFiUDP* ntpUDP;

#endif //ARDUINO_LIGHTFX_TIMEUTIL_H
