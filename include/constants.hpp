// Copyright (c) 2024 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <Arduino.h>

inline constexpr int turnOffSeq[] PROGMEM = {1, 1, 2, 2, 2, 3, 3, 3, 5, 5, 5, 7, 7, 7, 7, 10};
inline constexpr auto csAutoFxRoll PROGMEM = "autoFxRoll";
inline constexpr auto csStripBrightness PROGMEM = "stripBrightness";
inline constexpr auto csAudioThreshold PROGMEM = "audioThreshold";
inline constexpr auto csColorTheme PROGMEM = "colorTheme";
inline constexpr auto csAutoColorAdjust PROGMEM = "autoColorAdjust";
inline constexpr auto csRandomSeed PROGMEM = "randomSeed";
inline constexpr auto csCurFx PROGMEM = "curFx";
inline constexpr auto csSleepEnabled PROGMEM = "sleepEnabled";
inline constexpr auto csBrightness PROGMEM = "brightness";
inline constexpr auto csBrightnessLocked PROGMEM = "brightnessLocked";
inline constexpr auto csAuto PROGMEM = "auto";
inline constexpr auto csHoliday PROGMEM = "holiday";
inline constexpr auto strNR PROGMEM = "N/R";
inline constexpr auto csBroadcast PROGMEM = "broadcast";
inline constexpr auto sysCfgFileName PROGMEM = "/status/sysconfig.json";
inline constexpr auto calibFileName PROGMEM = "/status/calibration.json";
inline constexpr auto stateFileName PROGMEM = "/state.json";
inline constexpr auto sysFileName PROGMEM = "/sys.json";
inline constexpr auto strWakeup PROGMEM = "Wake-Up";
inline constexpr auto strBedtime PROGMEM = "Bed-time";
inline constexpr auto strNone PROGMEM = "None";
inline constexpr auto strParty PROGMEM = "Party";
inline constexpr auto strValentine PROGMEM = "ValentineDay";
inline constexpr auto strStPatrick PROGMEM = "StPatrick";
inline constexpr auto strMemorialDay PROGMEM = "MemorialDay";
inline constexpr auto strIndependenceDay PROGMEM = "IndependenceDay";
inline constexpr auto strHalloween PROGMEM = "Halloween";
inline constexpr auto strThanksgiving PROGMEM = "Thanksgiving";
inline constexpr auto strChristmas PROGMEM = "Christmas";
inline constexpr auto strNewYear PROGMEM = "NewYear";
inline constexpr auto strEffect PROGMEM = "effect";
inline constexpr auto csResetCal PROGMEM = "resetTempCal";
inline constexpr auto csBuildVersion PROGMEM = "buildVersion";
inline constexpr auto csBoardName PROGMEM = "boardName";
inline constexpr auto csBuildTime PROGMEM = "buildTime";
inline constexpr auto csScmBranch PROGMEM = "scmBranch";
inline constexpr auto csWdReboots PROGMEM = "wdReboots";
inline constexpr auto csBoardId PROGMEM = "boardId";
inline constexpr auto csSecElemId PROGMEM = "secElemId";
inline constexpr auto csMacAddress PROGMEM = "macAddress";
inline constexpr auto csWifiFwVersion PROGMEM = "wifiFwVersion";
inline constexpr auto csIpAddress PROGMEM = "ipAddress";
inline constexpr auto csGatewayAddress PROGMEM = "gatewayIpAddress";
inline constexpr auto csHeapSize PROGMEM = "heapSize";
inline constexpr auto csFreeHeap PROGMEM = "freeHeap";
inline constexpr auto csStackSize PROGMEM = "stackSize";
inline constexpr auto csFreeStack PROGMEM = "freeStack";
inline constexpr auto csStatus PROGMEM = "status";
inline constexpr auto csReady PROGMEM = "Rdy";
inline constexpr auto csBlocked PROGMEM = "Blk";
inline constexpr auto csSuspended PROGMEM = "Spn";
inline constexpr auto csDeleted PROGMEM = "Del";
inline constexpr auto csRunning PROGMEM = "Run";
inline constexpr auto csInvalid PROGMEM = "Inv";
inline constexpr auto csPowerOn PROGMEM = "power on";
inline constexpr auto csPinReset PROGMEM = "pin reset";
inline constexpr auto csSoftReset PROGMEM = "soft reset";
inline constexpr auto csWatchdog PROGMEM = "watchdog";
inline constexpr auto csDebug PROGMEM = "debug";
inline constexpr auto csGlitch PROGMEM = "glitch";
inline constexpr auto csBrownout PROGMEM = "brownout";

inline constexpr uint8_t dimmed = 20;
inline constexpr uint8_t maxChanges = 24;
extern const CRGB BKG;
extern const uint16_t dailyBedTime;
extern const uint16_t dailyWakeupTime;


#endif //CONSTANTS_H
