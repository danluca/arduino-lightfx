// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
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

void state_led_run();
void state_led_begin();
void logTaskStats();
void logSystemInfo();
void logSystemState();
void readSysInfo();
void saveSysInfo();

struct CRGB;

/**
 * Overall System information, covers both static (board/chip IDs) and dynamic (free memory, WiFi, status)
 */
class SysInfo {
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
    uint16_t status {0};
    bool cleanBoot {true};
    WatchdogQueue wdReboots{};   // keep only the last 10 watchdog reboots
    mutex_t mutex{};

protected:
    static void updateStateLED(uint32_t colorCode);
    static void updateStateLED(CRGB rgb);
    void updateStateLED() const;

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
    uint16_t setSysStatus(uint16_t bitMask);
    uint16_t resetSysStatus(uint16_t bitMask);
    [[nodiscard]] bool isSysStatus(uint16_t bitMask) const;
    [[nodiscard]] uint16_t getSysStatus() const;
    void setWiFiInfo(nina::WiFiClass & wifi);
    void setSecureElementId(const String & secId);
    void begin();
    static void sysConfig(JsonDocument &doc);
    static void heapStats(JsonObject &doc);
    static void taskStats(JsonObject &doc);
    static void setupStateLED();

    // JSON marshalling methods
    friend void readSysInfo();
    friend void saveSysInfo();
    friend void state_led_run();
    friend void state_led_begin();
};

extern SysInfo *sysInfo;

#endif //ARDUINO_LIGHTFX_SYSINFO_H
