//
// Copyright (c) 2023,2024,2025,2026 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef ARDUINO_LIGHTFX_UTIL_H
#define ARDUINO_LIGHTFX_UTIL_H

enum class SysStatus : uint16_t {
    None       = 0x0000,    // 0 << 0
    Setup0     = 0x0001,    // 1 << 0
    Setup1     = 0x0002,    // 1 << 1
    Filesystem = 0x0004,    // 1 << 2
    Wifi       = 0x0008,    // 1 << 3
    Ntp        = 0x0010,    // 1 << 4
    Dst        = 0x0020,    // 1 << 5
    Diag       = 0x0040     // 1 << 6
};

// Enable bitwise operations for SysStatus
inline SysStatus operator|(SysStatus a, SysStatus b) {
    return static_cast<SysStatus>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
inline SysStatus operator&(SysStatus a, SysStatus b) {
    return static_cast<SysStatus>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}
inline SysStatus operator^(SysStatus a, SysStatus b) {
    return static_cast<SysStatus>(static_cast<uint16_t>(a) ^ static_cast<uint16_t>(b));
}
inline SysStatus operator~(SysStatus a) {
    return static_cast<SysStatus>(~static_cast<uint16_t>(a));
}
inline SysStatus& operator|=(SysStatus& a, SysStatus b) {
    return a = a | b;
}
inline SysStatus& operator&=(SysStatus& a, SysStatus b) {
    return a = a & b;
}
inline SysStatus& operator^=(SysStatus& a, SysStatus b) {
    return a = a ^ b;
}
inline bool operator!(SysStatus a) {
    return static_cast<uint16_t>(a) == 0;
}

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
