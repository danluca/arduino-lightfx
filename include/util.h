// Copyright (c) 2023 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_UTIL_H
#define ARDUINO_LIGHTFX_UTIL_H

#include <Arduino.h>
#include <Arduino_LSM6DSOX.h>
#include <TimeLib.h>
#include "config.h"
#include "secrets.h"
#include "log.h"


#define IMU_TEMPERATURE_NOT_AVAILABLE   0.001f

#define SYS_STATUS_WIFI_MASK    0x01
#define SYS_STATUS_NTP_MASK     0x02
#define SYS_STATUS_DST_MASK     0x04
#define SYS_STATUS_MIC_MASK     0x08
#define SYS_STATUS_FILESYSTEM_MASK     0x10

extern const char stateFileName[];

uint8_t formatTime(char *buf, time_t time = 0);
uint8_t formatDate(char *buf, time_t time = 0);
uint8_t formatDateTime(char *buf, time_t time = 0);
bool isDST(time_t time);
float boardTemperature(bool bFahrenheit = false);
float chipTemperature(bool bFahrenheit = false);
float controllerVoltage();

uint16_t easeOutBounce(uint16_t x, uint16_t lim);
uint8_t bmul8(uint8_t a, uint8_t b);
uint8_t bscr8(uint8_t a, uint8_t b);
uint8_t bovl8(uint8_t a, uint8_t b);

void fsInit();

size_t readTextFile(const char *fname, String *s);
size_t writeTextFile(const char *fname, String *s);
bool removeFile(const char *fname);

const uint8_t setStatus(uint8_t bitMask);
const uint8_t resetStatus(uint8_t bitMask);
bool isStatus(uint8_t bitMask);
const uint8_t getStatus();

#endif //ARDUINO_LIGHTFX_UTIL_H
