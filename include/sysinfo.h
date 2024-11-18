// Copyright (c) 2024 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef ARDUINO_LIGHTFX_SYSINFO_H
#define ARDUINO_LIGHTFX_SYSINFO_H

#include <WiFiNINA.h>
#include <FreeRTOS.h>
#include <task.h>
#include "fixed_queue.h"
#include "log.h"
#include "config.h"

#define MAX_WATCHDOG_REBOOT_TIMESTAMPS  10      // max number of watchdog reboots to keep in the list
typedef FixedQueue<time_t, MAX_WATCHDOG_REBOOT_TIMESTAMPS> WatchdogQueue;

extern unsigned long prevStatTime;
extern unsigned long prevIdleTime;

void logTaskStats();
void logSystemInfo();
void readSysInfo();
void saveSysInfo();

/**
 * Overall System information, covers both static (board/chip IDs) and dynamic (free memory, WiFi, status)
 */
class SysInfo {
private:
    const String boardName;
    const String buildVersion;          // includes the commit sha
    const String buildTime;
    const String scmBranch;

    String boardId;
    String secElemId;
    String macAddress;
    String ipAddress;
    String gatewayIpAddress;
    String wifiFwVersion;
    String ssid;
    uint8_t status {0};
    bool cleanBoot {true};
    WatchdogQueue wdReboots;   // keep only the last 10 watchdog reboots
public:
    uint32_t freeHeap {0}, heapSize {0};
    uint32_t freeStack {0}, stackSize {0};
    uint32_t threadCount {0};

public:
    SysInfo();
    inline const String& getBoardName() const { return boardName; }
    inline const String& getBuildVersion() const { return buildVersion; }
    inline const String& getBuildTime() const { return buildTime; }
    inline const String& getScmBranch() const { return scmBranch; }
    inline const String& getBoardId() const { return boardId; }
    inline const String& getSecureElementId() const { return secElemId; }
    inline const String& getMacAddress() const { return macAddress; }
    inline const String& getIpAddress() const { return ipAddress; }
    inline const String& getGatewayIpAddress() const { return gatewayIpAddress; }
    inline const String& getWiFiFwVersion() const { return wifiFwVersion; }
    inline const String& getSSID() const { return ssid; }
    inline WatchdogQueue& watchdogReboots() { return wdReboots; }
    inline void markDirtyBoot() { cleanBoot = false; }
    inline bool isCleanBoot() const { return cleanBoot; }

    void fillBoardId();
    uint get_flash_capacity() const;
    uint8_t setSysStatus(uint8_t bitMask);
    uint8_t resetSysStatus(uint8_t bitMask);
    bool isSysStatus(uint8_t bitMask) const;
    uint8_t getSysStatus() const;
    void setWiFiInfo(WiFiClass & wifi);
    void setSecureElementId(const String & secId);

    // JSON marshalling methods
    friend void readSysInfo();
    friend void saveSysInfo();
};

extern SysInfo *sysInfo;



#endif //ARDUINO_LIGHTFX_SYSINFO_H
