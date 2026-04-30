// Copyright (c) by Dan Luca. All rights reserved.
//
#pragma once
#ifndef CONSTANTS_H
#define CONSTANTS_H

// #include <Arduino.h>

#define OTA_UPGRADE_NOTIFY 0xDC

struct CRGB;

inline constexpr int turnOffSeq[] = {1, 1, 2, 2, 2, 3, 3, 3, 5, 5, 5, 7, 7, 7, 7, 10};
inline constexpr auto csAutoFxRoll = "autoFxRoll";
inline constexpr auto csStripBrightness = "stripBrightness";
inline constexpr auto csColorTheme = "colorTheme";
inline constexpr auto csAutoColorAdjust = "autoColorAdjust";
inline constexpr auto csRandomSeed = "randomSeed";
inline constexpr auto csCurFx = "curFx";
inline constexpr auto csSleepEnabled = "sleepEnabled";
inline constexpr auto csBrightness = "brightness";
inline constexpr auto csBrightnessLocked = "brightnessLocked";
inline constexpr auto csAuto = "auto";
inline constexpr auto csHoliday = "holiday";
inline constexpr auto strNR = "N/R";
inline constexpr auto csBroadcast = "broadcast";
inline constexpr auto csIgnoreWebFx = "ignoreWebFx";
inline constexpr auto kHeaderXSource = "X-Source";  // HTTP header constants for origin discrimination
inline constexpr auto kXSourceUi = "ui";
inline constexpr auto kXSourceBoard = "board";
inline constexpr auto kHeaderUserAgent = "User-Agent";
inline constexpr auto kUaBoardPrefix = "rp2040-lightfx-master";     // User-Agent prefix used by board-originated sync/broadcast requests
inline constexpr auto fxCfgFileName = "/status/fxconfig.json";
inline constexpr auto sysCfgFileName = "/status/sysconfig.json";
inline constexpr auto calibFileName = "/status/calibration.json";
inline constexpr auto stateFileName = "/state.json";
inline constexpr auto sysFileName = "/sys.json";
inline constexpr auto strWakeup = "Wake-Up";
inline constexpr auto strBedtime = "Bed-time";
inline constexpr auto strNone = "None";
inline constexpr auto strParty = "Party";
inline constexpr auto strValentine = "ValentineDay";
inline constexpr auto strStPatrick = "StPatrick";
inline constexpr auto strMemorialDay = "MemorialDay";
inline constexpr auto strIndependenceDay = "IndependenceDay";
inline constexpr auto strHalloween = "Halloween";
inline constexpr auto strThanksgiving = "Thanksgiving";
inline constexpr auto strChristmas = "Christmas";
inline constexpr auto strNewYear = "NewYear";
inline constexpr auto strEffect = "effect";
inline constexpr auto csResetCal = "resetTempCal";
inline constexpr auto csBuildVersion = "buildVersion";
inline constexpr auto csDeviceName = "deviceName";
inline constexpr auto csBoardName = "boardName";
inline constexpr auto csBuildTime = "buildTime";
inline constexpr auto csScmBranch = "scmBranch";
inline constexpr auto csWdReboots = "wdReboots";
inline constexpr auto csBoardId = "boardId";
inline constexpr auto csSecElemId = "secElemId";
inline constexpr auto csMacAddress = "macAddress";
inline constexpr auto csWifiFwVersion = "wifiFwVersion";
inline constexpr auto csIpAddress = "ipAddress";
inline constexpr auto csGatewayAddress = "gatewayIpAddress";
inline constexpr auto csHeapSize = "heapSize";
inline constexpr auto csFreeHeap = "freeHeap";
inline constexpr auto csStackSize = "stackSize";
inline constexpr auto csFreeStack = "freeStack";
inline constexpr auto csStatus = "status";
inline constexpr auto csReady = "Rdy";
inline constexpr auto csBlocked = "Blk";
inline constexpr auto csSuspended = "Spn";
inline constexpr auto csDeleted = "Del";
inline constexpr auto csRunning = "Run";
inline constexpr auto csInvalid = "Inv";
inline constexpr auto csPowerOn = "power on";
inline constexpr auto csPinReset = "pin reset";
inline constexpr auto csSoftReset = "soft reset";
inline constexpr auto csWatchdog = "watchdog";
inline constexpr auto csDebug = "debug";
inline constexpr auto csGlitch = "glitch";
inline constexpr auto csBrownout = "brownout";
inline constexpr auto csCORE0 = "CORE0";
inline constexpr auto csCORE1 = "CORE1";
inline constexpr auto csFxTask = "Fx";
inline constexpr auto csFWImageFilename = "/fw.bin";
inline constexpr auto healthEventFileName = "/status/health_event.json";
// Soft-reset markers stored in watchdog scratch register.
inline constexpr uint32_t kResetMarkerNone = 0u;
inline constexpr uint32_t kResetMarkerPanic = 0xA11CE520u;
inline constexpr uint32_t kResetMarkerAssert = 0xA11CE521u;
inline constexpr uint32_t kResetMarkerHardFault = 0xA11CE522u;
inline constexpr uint32_t kResetMarkerMalloc = 0xA11CE523u;
inline constexpr uint32_t kResetMarkerStackOverflow = 0xA11CE524u;
inline constexpr uint32_t kResetMarkerFxStall = 0xA11CE525u;
inline constexpr uint32_t kResetMarkerCore0Stall = 0xA11CE526u;
inline constexpr uint32_t kResetMarkerOta = 0xA11CE502u;
inline constexpr uint32_t kResetMarkerReboot = 0xA11CE503u;
inline constexpr uint32_t kResetMarkerUnknown = 0xA11CE504u;
inline constexpr uint8_t kResetMarkerScratchIndex = 7u;
inline constexpr uint8_t kFxHeartbeatScratchIndex = 6u;
inline constexpr uint8_t kFxStageScratchIndex = 5u;
inline constexpr uint8_t kCore0HeartbeatScratchIndex = 4u;
inline constexpr uint8_t kCore1HeartbeatScratchIndex = 3u;
inline constexpr uint8_t kFsBlockedScratchIndex = 2u;
inline constexpr uint32_t kFsBlockedMagic = 0xFB000000u;

// Diagnostic toggles (temporary for watchdog investigation).
#ifndef DIAG_CORE_HEARTBEATS
#define DIAG_CORE_HEARTBEATS 1
#endif
#ifndef DIAG_WDT_PING_CORE0
#define DIAG_WDT_PING_CORE0 1
#endif

// FX task stage markers stored in watchdog scratch register.
inline constexpr uint32_t kFxStageNone = 0x00000000u;
inline constexpr uint32_t kFxStageEnter = 0xF0000001u;
inline constexpr uint32_t kFxStageAfterQueue = 0xF0000002u;
inline constexpr uint32_t kFxStageAfterOtaCheck = 0xF0000003u;
inline constexpr uint32_t kFxStageFirmwareUpgrade = 0xF0000004u;
inline constexpr uint32_t kFxStageBeforeLoop = 0xF0000005u;
inline constexpr uint32_t kFxStageAfterLoop = 0xF0000006u;
inline constexpr uint32_t kFxStageAfterPing = 0xF0000007u;

inline constexpr uint8_t dimmed = 20;
inline constexpr uint8_t maxChanges = 24;
extern const CRGB BKG;
extern const uint16_t dailyBedTime;
extern const uint16_t dailyWakeupTime;


#endif //CONSTANTS_H
