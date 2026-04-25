// Copyright (c) by Dan Luca. All rights reserved.
//
#pragma once
#ifndef ARDUINO_LIGHTFX_SYSINFO_H
#define ARDUINO_LIGHTFX_SYSINFO_H

#include <WiFi.h>
#include <ArduinoJson.h>
#include <task.h>
#include <functional>
#include <vector>
#include "../lib/Utils/src/fixed_queue.h"
#include "util.h"

#define MAX_WATCHDOG_REBOOT_TIMESTAMPS  10      // max number of watchdog reboots to keep in the list

struct WatchdogRebootInfo {
    time_t time {0};
    const char *resetReason {nullptr};
    const char *marker {nullptr};
    const char *fxStage {nullptr};
    const char *fsBlockedAction {nullptr};
};
typedef FixedQueue<WatchdogRebootInfo, MAX_WATCHDOG_REBOOT_TIMESTAMPS> WatchdogQueue;

extern unsigned long prevStatTime;
extern unsigned long prevIdleTime;

void state_led_update();
void state_led_begin();
bool captureTaskRuntimeSnapshot(bool force = false);
void logTaskStats();
void logTaskSummary();
void logHeapStats();
size_t getUsedHeapBytes();
void logSystemInfo();
void logSystemState();
void readSysInfo();
void saveSysInfo();
void saveSlownessHealthEvent(uint32_t c0Diff, uint32_t c1Diff, uint32_t fxDiff, bool isStall);
void saveRebootHealthEvent();
const char *taskStatusToString(eTaskState state);

struct CRGB;

/**
 * Overall System information, covers both static (board/chip IDs) and dynamic (free memory, WiFi, status)
 */
class SysInfo {
    const String boardName;
    const String deviceName;
    const String buildVersion;          // includes the commit sha
    const String buildTime;
    const String scmBranch;

    String cpuModel;
    uint8_t cpuVersion;
    int cpuFrequency;   // in Hz
    size_t psramSize;
    String boardId;
    String secElemId;
    String macAddress;
    String strIpAddress;
    String strGatewayIpAddress;
    String wifiFwVersion;
    String ssid;
    IPAddress ipAddress;
    IPAddress ipGateway;
    SysStatus status {SysStatus::None};
    bool cleanBoot {true};
    WatchdogQueue wdReboots{};   // keep only the last 10 watchdog reboots
    mutable mutex_t mutex{};

protected:
    static void updateBoardLED(uint32_t colorCode);
    static void updateBoardLED(CRGB rgb);
    void updateStatusLED() const;

public:
    uint32_t freeHeap {0}, heapSize {0};
    uint32_t freeStack {0}, stackSize {0};
    uint32_t threadCount {0};

    SysInfo();
    [[nodiscard]] const String& getDeviceName() const { return deviceName; }
    [[nodiscard]] const String& getBoardType() const { return boardName; }
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
    [[nodiscard]] int getCPUFrequency() const { return cpuFrequency; }
    void addWatchdogReboot(const WatchdogRebootInfo& info);
    [[nodiscard]] size_t watchdogRebootsCount() const;
    [[nodiscard]] bool hasWatchdogReboots() const;
    [[nodiscard]] time_t lastWatchdogReboot() const;
    [[nodiscard]] std::vector<WatchdogRebootInfo> watchdogRebootsSnapshot() const;
    void transformWatchdogReboots(const std::function<time_t(time_t)>& transform);
    void markDirtyBoot() { cleanBoot = false; }
    [[nodiscard]] bool isCleanBoot() const { return cleanBoot; }
    IPAddress& refIpAddress() { return ipAddress; }
    IPAddress& refGatewayIpAddress() { return ipGateway; }

    void fillBoardId();
    [[nodiscard]] uint get_flash_capacity() const;
    SysStatus setSysStatus(SysStatus bitMask);
    SysStatus resetSysStatus(SysStatus bitMask);
    [[nodiscard]] bool isSysStatus(SysStatus bitMask) const;
    [[nodiscard]] SysStatus getSysStatus() const;
    void setWiFiInfo(::WiFiClass & wifi);
    void setSecureElementId(const String & secId);
    void begin();
    static void sysConfig(JsonDocument &doc);
    static void heapStats(JsonObject &doc);
    static void taskStats(JsonObject &doc);
    static void setupStateLED();

    // JSON marshalling methods
    friend class SysInfoPersistence;
    friend void readSysInfo();
    friend void saveSysInfo();
    friend void state_led_update();
    friend void state_led_begin();
};

extern SysInfo *sysInfo;

#endif //ARDUINO_LIGHTFX_SYSINFO_H
