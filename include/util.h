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


#define IMU_TEMPERATURE_NOT_AVAILABLE   0.001

extern const char stateFileName[];

uint8_t formatTime(char *buf, time_t time = 0);
uint8_t formatDate(char *buf, time_t time = 0);
uint8_t formatDateTime(char *buf, time_t time = 0);
float boardTemperature(bool bFahrenheit = false);
float chipTemperature();
float controllerVoltage();

uint16_t easeOutBounce(uint16_t x, uint16_t lim);
uint8_t bmul8(uint8_t a, uint8_t b);
uint8_t bscr8(uint8_t a, uint8_t b);
uint8_t bovl8(uint8_t a, uint8_t b);

void fsInit();

size_t readTextFile(const char *fname, String *s);
size_t writeTextFile(const char *fname, String *s);
bool removeFile(const char *fname);


#endif //ARDUINO_LIGHTFX_UTIL_H
