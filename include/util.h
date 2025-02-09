//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_UTIL_H
#define ARDUINO_LIGHTFX_UTIL_H

#define SYS_STATUS_WIFI    0x01
#define SYS_STATUS_NTP     0x02
#define SYS_STATUS_DST     0x04
#define SYS_STATUS_MIC     0x08
#define SYS_STATUS_FILESYSTEM     0x10
#define SYS_STATUS_ECC     0x20
#define SYS_STATUS_DIAG    0x40

enum MiscAction:uint8_t {ALARM_SETUP, ALARM_CHECK, SAVE_SYS_INFO};
enum CommAction:uint8_t {WIFI_ENSURE};

ulong adcRandom();

uint8_t bmul8(uint8_t a, uint8_t b);
uint8_t bscr8(uint8_t a, uint8_t b);
uint8_t bovl8(uint8_t a, uint8_t b);
bool rblend8(uint8_t &a, uint8_t b, uint8_t amt=22) ;

uint8_t secRandom8(uint8_t minLim = 0, uint8_t maxLim = 0);
uint16_t secRandom16(uint16_t minLim = 0, uint16_t maxLim = 0);
uint32_t secRandom(uint32_t minLim = 0, uint32_t maxLim = 0);
bool secElement_setup();
void watchdogSetup();
void watchdogPing();
void taskDelay(uint32_t ms);

#endif //ARDUINO_LIGHTFX_UTIL_H
