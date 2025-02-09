// Copyright (c) 2024 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef ARDUINO_LIGHTFX_SYSINFO_H
#define ARDUINO_LIGHTFX_SYSINFO_H

#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include "fixed_queue.h"

#define MAX_WATCHDOG_REBOOT_TIMESTAMPS  10      // max number of watchdog reboots to keep in the list
typedef FixedQueue<time_t, MAX_WATCHDOG_REBOOT_TIMESTAMPS> WatchdogQueue;

extern unsigned long prevStatTime;
extern unsigned long prevIdleTime;

void logTaskStats();
void logSystemInfo();
void logSystemState();
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
    String strIpAddress;
    String strGatewayIpAddress;
    String wifiFwVersion;
    String ssid;
    IPAddress ipAddress;
    IPAddress ipGateway;
    uint8_t status {0};
    bool cleanBoot {true};
    WatchdogQueue wdReboots;   // keep only the last 10 watchdog reboots

public:
    uint32_t freeHeap {0}, heapSize {0};
    uint32_t freeStack {0}, stackSize {0};
    uint32_t threadCount {0};

    SysInfo();
    [[nodiscard]] const String& getBoardName() const { return boardName; }
    [[nodiscard]] const String& getBuildVersion() const { return buildVersion; }
    [[nodiscard]] const String& getBuildTime() const { return buildTime; }
    [[nodiscard]] const String& getScmBranch() const { return scmBranch; }
    [[nodiscard]] const String& getBoardId() const { return boardId; }
    [[nodiscard]] const String& getSecureElementId() const { return secElemId; }
    [[nodiscard]] const String& getMacAddress() const { return macAddress; }
    [[nodiscard]] const String& getIpAddress() const { return strIpAddress; }
    [[nodiscard]] const String& getGatewayIpAddress() const { return strGatewayIpAddress; }
    [[nodiscard]] const String& getWiFiFwVersion() const { return wifiFwVersion; }
    [[nodiscard]] const String& getSSID() const { return ssid; }
    WatchdogQueue& watchdogReboots() { return wdReboots; }
    void markDirtyBoot() { cleanBoot = false; }
    [[nodiscard]] bool isCleanBoot() const { return cleanBoot; }
    IPAddress& refIpAddress() { return ipAddress; }
    IPAddress& refGatewayIpAddress() { return ipGateway; }

    void fillBoardId();
    [[nodiscard]] uint get_flash_capacity() const;
    uint8_t setSysStatus(uint8_t bitMask);
    uint8_t resetSysStatus(uint8_t bitMask);
    [[nodiscard]] bool isSysStatus(uint8_t bitMask) const;
    [[nodiscard]] uint8_t getSysStatus() const;
    void setWiFiInfo(nina::WiFiClass & wifi);
    void setSecureElementId(const String & secId);
    static void sysConfig(JsonDocument &doc);
    static void heapStats(JsonObject &doc);
    static void taskStats(JsonObject &doc);

    // JSON marshalling methods
    friend void readSysInfo();
    friend void saveSysInfo();
};

extern SysInfo *sysInfo;

#endif //ARDUINO_LIGHTFX_SYSINFO_H
