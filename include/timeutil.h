// Copyright (c) 2023 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_TIMEUTIL_H
#define ARDUINO_LIGHTFX_TIMEUTIL_H

#include <WiFiNINA.h>
#include <NTPClient.h>
#include <TimeLib.h>

#define CST_OFFSET_SECONDS (-21600)   //Central Standard Time - America/Chicago
#define CDT_OFFSET_SECONDS  (-18000)  //Central Daylight Time - America/Chicago

extern WiFiUDP Udp;
extern NTPClient timeClient;

enum Holiday { None, Party, Halloween, Thanksgiving, Christmas, NewYear };
Holiday buildHoliday(time_t time);
Holiday currentHoliday();
Holiday parseHoliday(const String *str);
const char *holidayToString(Holiday hday);

uint8_t formatTime(char *buf, time_t time = 0);
uint8_t formatDate(char *buf, time_t time = 0);
uint8_t formatDateTime(char *buf, time_t time = 0);
bool isDST(time_t time);
uint16_t encodeMonthDay(time_t time = 0);

bool time_setup();
time_t curUnixTime();
bool ntp_sync();

#endif //ARDUINO_LIGHTFX_TIMEUTIL_H
