//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef ARDUINO_LIGHTFX_UTIL_H
#define ARDUINO_LIGHTFX_UTIL_H

#define SYS_STATUS_SETUP0       0x0001
#define SYS_STATUS_SETUP1       0x0002
#define SYS_STATUS_FILESYSTEM   0x0004
#define SYS_STATUS_WIFI         0x0008
#define SYS_STATUS_NTP          0x0010
#define SYS_STATUS_DST          0x0020
#define SYS_STATUS_DIAG         0x0040

#define TWENTY_TWENTY    1577836800L    //2020-01-01 00:00
#define TWENTY_SEVENTY   3155760000L    //2070-01-01 00:00 - if this code is still relevant in 2070, something is wrong...

ulong adcRandom();

uint8_t bmul8(uint8_t a, uint8_t b);
uint8_t bscr8(uint8_t a, uint8_t b);
uint8_t bovl8(uint8_t a, uint8_t b);
bool rblend8(uint8_t &a, uint8_t b, uint8_t amt=22) ;

uint8_t secRandom8(uint8_t minLim = 0, uint8_t maxLim = 0);
uint16_t secRandom16(uint16_t minLim = 0, uint16_t maxLim = 0);
uint32_t secRandom(uint32_t minLim = 0, uint32_t maxLim = 0);
void watchdogSetup();
void watchdogPing();
void taskDelay(uint32_t ms);
void holidayUpdate();

#endif //ARDUINO_LIGHTFX_UTIL_H
