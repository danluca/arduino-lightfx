// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//
#include <Arduino.h>
#include <ArduinoJson.h>
#include <memory>

#include "config.h"
#include "constants.hpp"
#include "filesystem.h"
#include "log.h"
#include "sysinfo_internal.h"
#include "version.h"

namespace {

constexpr uint32_t kSysInfoSaveIntervalMs = 300u * 1000u;
SysInfoPersistence g_sysInfoPersistence{};

} // namespace

SysInfoPersistence &SysInfoPersistence::instance() {
    return g_sysInfoPersistence;
}

void SysInfoPersistence::markDirty() {
    dirty_ = true;
}

void SysInfo::sysConfig(JsonDocument &doc) {
    doc["arduinoPicoVersion"] = ARDUINO_PICO_VERSION_STR;
    doc["freeRTOSVersion"] = tskKERNEL_VERSION_NUMBER;
    doc[csBoardName] = sysInfo->boardName;
    doc[csDeviceName] = sysInfo->deviceName;
    doc[csBoardId] = sysInfo->boardId;
    doc["psramSize"] = sysInfo->psramSize;
    doc["cpuModel"] = sysInfo->cpuModel;
    doc["cpuVersion"] = sysInfo->cpuVersion;
    doc["cpuFrequency"] = sysInfo->cpuFrequency;
    doc["cpuCore"] = RP2040::cpuid();
    doc[csSecElemId] = sysInfo->secElemId;
    doc[csBuildVersion] = sysInfo->buildVersion;
    doc[csScmBranch] = sysInfo->scmBranch;
    doc[csBuildTime] = sysInfo->buildTime;
    doc[csWifiFwVersion] = sysInfo->wifiFwVersion;
    doc["wifiLatestVersion"] = WIFI_FIRMWARE_LATEST_VERSION;
    doc[csMacAddress] = sysInfo->macAddress;
    doc[csIpAddress] = sysInfo->strIpAddress;
    doc[csGatewayAddress] = sysInfo->strGatewayIpAddress;
    doc[csStatus] = static_cast<uint16_t>(sysInfo->status);
    doc[csHeapSize] = sysInfo->heapSize;
    doc[csFreeHeap] = sysInfo->freeHeap;
    doc[csStackSize] = sysInfo->stackSize;
    doc[csFreeStack] = sysInfo->freeStack;
    const auto reboots = doc[csWdReboots].to<JsonArray>();
    for (const auto &t : sysInfo->watchdogRebootsSnapshot())
        (void)reboots.add(t);
}

void SysInfoPersistence::read() {
    String json;
    json.reserve(512);
    if (const size_t sysSize = SyncFsImpl.readFile(sysFileName, &json); sysSize > 0) {
        log_info(F("System information [%s]:\n%s"), sysFileName, json.c_str());
        JsonDocument doc;
        if (const DeserializationError error = deserializeJson(doc, json)) {
            log_error(F("Error reading the system information JSON file %s [%zu bytes]: %s - system information state NOT restored. Content read:\n%s"), sysFileName, sysSize, error.c_str(), json.c_str());
            doc.clear();
            return;
        }

        const String bldVersion = doc[csBuildVersion];
        const String brdName = doc[csBoardName];
        const String devName = doc[csDeviceName] | DEVICE_NAME;
        const String bldTime = doc[csBuildTime];
        const auto gitBranch = doc[csScmBranch].as<String>();
        if (bldVersion.equals(sysInfo->buildVersion) && doc[csWdReboots].is<JsonArray>()) {
            const auto wdReboots = doc[csWdReboots].as<JsonArray>();
            for (JsonVariant i : wdReboots)
                sysInfo->addWatchdogReboot(i.as<time_t>());
        } else {
            log_warn(F("Build version change detected - previous watchdog reboot timestamps %s have been discarded"), doc[csWdReboots].as<String>().c_str());
        }
        sysInfo->boardId = doc[csBoardId].as<String>();
        sysInfo->secElemId = doc[csSecElemId].as<String>();
        sysInfo->macAddress = doc[csMacAddress].as<String>();
        sysInfo->wifiFwVersion = doc[csWifiFwVersion].as<String>();
        sysInfo->strIpAddress = doc[csIpAddress].as<String>();
        sysInfo->strGatewayIpAddress = doc[csGatewayAddress].as<String>();
        sysInfo->heapSize = doc[csHeapSize];
        sysInfo->freeHeap = doc[csFreeHeap];
        sysInfo->stackSize = doc[csStackSize];
        sysInfo->freeStack = doc[csFreeStack];
        const auto lastStatus = doc[csStatus].as<uint16_t>();
        log_info(F("System Information restored from %s [%d bytes]: boardName=%s, deviceName=%s, buildVersion=%s, buildTime=%s, scmBranch=%s, boardId=%s, secElemId=%s, macAddress=%s, status=%#hhX (last %#hhX), IP=%s, Gateway=%s"),
                   sysFileName, sysSize, brdName.c_str(), devName.c_str(), bldVersion.c_str(), bldTime.c_str(), gitBranch.c_str(), sysInfo->boardId.c_str(), sysInfo->secElemId.c_str(), sysInfo->macAddress.c_str(), sysInfo->status,
                   lastStatus, sysInfo->strIpAddress.c_str(), sysInfo->strGatewayIpAddress.c_str());
        (void)lastStatus;
        doc.clear();
    } else {
        log_info(F("System information file %s not found - system information will be re-built"), sysFileName);
    }
}

void SysInfoPersistence::save() {
    const uint32_t nowMs = millis();
    if (!dirty_ && lastSaveMs_ != 0 && (nowMs - lastSaveMs_) < kSysInfoSaveIntervalMs)
        return;

    JsonDocument doc;
    SysInfo::sysConfig(doc);

    auto str = std::make_unique<String>();
    str->reserve(measureJson(doc));
    serializeJson(doc, *str);

    if (SyncFsImpl.writeFileAsync(sysFileName, str.get())) {
        dirty_ = false;
        lastSaveMs_ = nowMs;
        (void)str.release();
    } else {
        log_error(F("Failed to enqueue async system information file write %s"), sysFileName);
    }

    doc.clear();
}

void readSysInfo() {
    SysInfoPersistence::read();
}

void saveSysInfo() {
    SysInfoPersistence::instance().save();
}
