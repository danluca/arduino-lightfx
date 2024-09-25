//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_UTIL_H
#define ARDUINO_LIGHTFX_UTIL_H

#include <Arduino.h>
#include <Arduino_LSM6DSOX.h>
#include <ArduinoECCX08.h>
#include "circular_buffer.h"
#include "fixed_queue.h"
#include "timeutil.h"
#include "config.h"
#include "secrets.h"
#include "log.h"
#include "sysinfo.h"

#define IMU_TEMPERATURE_NOT_AVAILABLE   0.001f
#define TEMP_NA_COMPARE_EPSILON      0.0001f

#define SYS_STATUS_WIFI    0x01
#define SYS_STATUS_NTP     0x02
#define SYS_STATUS_DST     0x04
#define SYS_STATUS_MIC     0x08
#define SYS_STATUS_FILESYSTEM     0x10
#define SYS_STATUS_ECC     0x20

extern const char stateFileName[];
extern const char sysFileName[];

float boardTemperature(bool bFahrenheit = false);
float chipTemperature(bool bFahrenheit = false);
inline static float toFahrenheit(float celsius) {
    return celsius * 9.0f / 5 + 32;
}

float controllerVoltage();
ulong adcRandom();
void setupStateLED();
void updateStateLED(uint32_t colorCode);
void updateStateLED(uint8_t red, uint8_t green, uint8_t blue);

uint8_t bmul8(uint8_t a, uint8_t b);
uint8_t bscr8(uint8_t a, uint8_t b);
uint8_t bovl8(uint8_t a, uint8_t b);
bool rblend8(uint8_t &a, uint8_t b, uint8_t amt=22) ;

void fsInit();

size_t readTextFile(const char *fname, String *s);
size_t writeTextFile(const char *fname, String *s);
bool removeFile(const char *fname);

uint8_t setSysStatus(uint8_t bitMask);
uint8_t resetSysStatus(uint8_t bitMask);
bool isSysStatus(uint8_t bitMask);
uint8_t getSysStatus();

uint8_t secRandom8(uint8_t minLim = 0, uint8_t maxLim = 0);
uint16_t secRandom16(uint16_t minLim = 0, uint16_t maxLim = 0);
uint32_t secRandom(uint32_t minLim = 0, uint32_t maxLim = 0);
bool secElement_setup();
void watchdogSetup();
void watchdogPing();

extern FixedQueue<TimeSync, 8> timeSyncs;
extern CircularBuffer<short> *audioData;

#endif //ARDUINO_LIGHTFX_UTIL_H
