//
// Copyright (c) by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_TIMEUTIL_H
#define ARDUINO_LIGHTFX_TIMEUTIL_H

#include <WiFi.h>
#include <TimeLib.h>
#include <fixed_queue.h>

enum Holiday { None, Party, ValentineDay, StPatrick, MemorialDay, IndependenceDay, Halloween, Thanksgiving, Christmas, NewYear };
Holiday buildHoliday(time_t time);
Holiday currentHoliday();
Holiday parseHoliday(const String *str);
const char *holidayToString(Holiday hday);

bool isDST(time_t time);
uint16_t encodeMonthDay(time_t time = 0);

// how long the system clock may go without an NTP sync before NTP is considered lost - the lwIP SNTP poll interval is 12 hours (SNTP_UPDATE_DELAY, a build flag in platformio.ini)
constexpr time_t NTP_STALE_SECS = 36 * SECS_PER_HOUR;
// how long the boot sequence waits for the first NTP sync before carrying on (the retry timer picks it up later)
constexpr uint32_t NTP_BOOT_WAIT_MS = 10000;

void timeBegin();
bool timeSynchronized();
bool timeSetup(uint32_t waitMs = 0);
void updateLoggingTimebase();

#endif //ARDUINO_LIGHTFX_TIMEUTIL_H
