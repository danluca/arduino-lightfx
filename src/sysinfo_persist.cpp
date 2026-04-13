// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//
#include <Arduino.h>
#include <ArduinoJson.h>
#include <hardware/watchdog.h>
#include <memory>

#include "config.h"
#include "constants.hpp"
#include "filesystem.h"
#include "log.h"
#include "sysinfo_internal.h"
#include "task_msg.h"
#include "TimeFormat.h"
#include "TimeService.h"
#include "version.h"

namespace {

constexpr uint32_t kSysInfoSaveIntervalMs = 300u * 1000u;
SysInfoPersistence g_sysInfoPersistence{};

} // namespace

SysInfoPersistence &SysInfoPersistence::instance() {
    return g_sysInfoPersistence;
}

void SysInfoPersistence::markDirty() {
    CoreMutex lock(&mutex_);
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
    {
        CoreMutex lock(&mutex_);
        if (writePending_)
            return;
        if (!dirty_ && lastSaveMs_ != 0 && (nowMs - lastSaveMs_) < kSysInfoSaveIntervalMs)
            return;

        writePending_ = true;
        dirty_ = false;
    }

    JsonDocument doc;
    SysInfo::sysConfig(doc);

    auto str = std::make_unique<String>();
    str->reserve(measureJson(doc));
    serializeJson(doc, *str);

    if (SyncFsImpl.writeFileAsync(sysFileName, str.get(), almQueue, AlmAction::SAVE_SYS_INFO_DONE)) {
        CoreMutex lock(&mutex_);
        lastSaveMs_ = nowMs;
    } else {
        CoreMutex lock(&mutex_);
        writePending_ = false;
        dirty_ = true;
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

/**
 * Writes the "slowness" section of the health event file when the health monitor detects task slowness or a stall.
 * Reads the existing file first to preserve the "reboot" section, then overwrites with the updated slowness data.
 * The top 3 non-idle tasks by CPU delta percentage in the last interval are included.
 * File write is asynchronous so as not to block the caller.
 * @param c0Diff milliseconds since last CORE0 check-in
 * @param c1Diff milliseconds since last CORE1 check-in
 * @param fxDiff milliseconds since last FX check-in
 * @param isStall true when the watchdog is no longer being updated (task stalled), false for a slowness warning
 */
void saveSlownessHealthEvent(const uint32_t c0Diff, const uint32_t c1Diff, const uint32_t fxDiff, const bool isStall) {
    TaskRuntimeSnapshot current, previous;
    if (!TaskRuntimeMonitor::instance().load(current, previous))
        return;

    const uint64_t totalDelta = runtimeDelta(current.totalRunTime, previous.totalRunTime);

    // Find top 3 non-idle tasks by CPU delta percentage
    struct TopTask { const char *name = nullptr; float pct = 0.0f; };
    TopTask top[3];
    for (const auto &task : current.tasks) {
        if (isIdleTaskName(task.pcTaskName)) continue;
        const TaskStatus_t *prev = findTaskStatus(previous.tasks, task.xTaskNumber);
        const uint64_t delta = prev
            ? runtimeDelta(task.ulRunTimeCounter, prev->ulRunTimeCounter)
            : task.ulRunTimeCounter;
        const float pct = runtimePct(delta, totalDelta);
        for (int i = 0; i < 3; i++) {
            if (pct > top[i].pct) {
                for (int j = 2; j > i; j--) top[j] = top[j - 1];
                top[i] = {task.pcTaskName, pct};
                break;
            }
        }
    }

    // Read existing file to preserve the reboot section
    String existing;
    JsonDocument doc;
    if (SyncFsImpl.readFile(healthEventFileName, &existing) > 0)
        deserializeJson(doc, existing);

    const time_t curTime = now();
    auto slowness = doc["slowness"].to<JsonObject>();
    slowness["type"] = isStall ? "stall" : "slowness";
    slowness["uptimeMs"] = current.capturedAtMs;
    slowness["cpuLoadPct"] = cpuLoadPct(current, previous);
    slowness["windowSec"] = snapshotWindowSec(current, previous);
    slowness["curMillis"] = millis();
    slowness["curDate"] = TimeFormat::dateAsString(curTime);
    slowness["curTime"] = TimeFormat::timeAsString(curTime);
    const auto diffs = slowness["diffs"].to<JsonObject>();
    diffs["c0"] = c0Diff;
    diffs["c1"] = c1Diff;
    diffs["fx"] = fxDiff;
    const auto topArr = slowness["topTasks"].to<JsonArray>();
    for (const auto &t : top) {
        if (t.name == nullptr) break;
        auto jt = topArr.add<JsonObject>();
        jt["name"] = t.name;
        jt["cpuPct"] = t.pct;
    }

    String out;
    out.reserve(measureJson(doc));
    serializeJson(doc, out);
    doc.clear();

    if (!SyncFsImpl.writeFileAsync(healthEventFileName, &out))
        log_error(F("Failed to enqueue health event file write to %s"), healthEventFileName);
}

/**
 * Writes the "reboot" section of the health event file using the watchdog scratch registers that hold
 * the reset cause markers. Must be called before logSystemInfo() clears those registers.
 * Reads the existing file first to preserve the "slowness" section.
 * Only writes when the reset was watchdog-triggered or a non-none reset marker is present.
 * File write is synchronous since this runs early in the boot sequence.
 */
void saveRebootHealthEvent() {
    const RP2040::resetReason_t resetReason = rp2040.getResetReason();
    const uint32_t resetMarker = watchdog_hw->scratch[kResetMarkerScratchIndex];
    if (resetReason != RP2040::WDT_RESET && resetMarker == kResetMarkerNone)
        return;

    const uint32_t fxStage = watchdog_hw->scratch[kFxStageScratchIndex];
    const uint32_t fsBlocked = watchdog_hw->scratch[kFsBlockedScratchIndex];

    auto resetReasonStr = [](const RP2040::resetReason_t r) -> const char * {
        switch (r) {
            case RP2040::WDT_RESET:     return csWatchdog;
            case RP2040::PWRON_RESET:   return csPowerOn;
            case RP2040::RUN_PIN_RESET: return csPinReset;
            case RP2040::SOFT_RESET:    return csSoftReset;
            case RP2040::DEBUG_RESET:   return csDebug;
            default:                    return "unknown";
        }
    };
    auto markerStr = [](const uint32_t m) -> const char * {
        switch (m) {
            case kResetMarkerNone:          return "none";
            case kResetMarkerPanic:         return "panic";
            case kResetMarkerAssert:        return "assert";
            case kResetMarkerHardFault:     return "hardfault";
            case kResetMarkerMalloc:        return "malloc_failed";
            case kResetMarkerStackOverflow: return "stack_overflow";
            case kResetMarkerFxStall:       return "fx_stall";
            case kResetMarkerOta:           return "ota";
            case kResetMarkerReboot:        return "reboot";
            default:                        return "unknown";
        }
    };
    auto stageStr = [](const uint32_t s) -> const char * {
        switch (s) {
            case kFxStageNone:             return "none";
            case kFxStageEnter:            return "enter";
            case kFxStageAfterQueue:       return "after_queue";
            case kFxStageAfterOtaCheck:    return "after_ota_check";
            case kFxStageFirmwareUpgrade:  return "fw_upgrade";
            case kFxStageBeforeLoop:       return "before_loop";
            case kFxStageAfterLoop:        return "after_loop";
            case kFxStageAfterPing:        return "after_ping";
            default:                       return "unknown";
        }
    };

    // Read existing file to preserve the slowness section
    String existing;
    JsonDocument doc;
    if (SyncFsImpl.readFile(healthEventFileName, &existing) > 0)
        deserializeJson(doc, existing);

    auto reboot = doc["reboot"].to<JsonObject>();
    reboot["uptimeMs"] = millis();
    reboot["resetReason"] = resetReasonStr(resetReason);
    reboot["resetMarker"] = markerStr(resetMarker);
    reboot["fxStage"] = stageStr(fxStage);
    if ((fsBlocked & 0xFF000000u) == kFsBlockedMagic) {
        static constexpr const char *fsActions[] = {
            "READ_FILE", "WRITE_FILE", "WRITE_FILE_ASYNC", "APPEND_FILE", "APPEND_FILE_BIN",
            "RENAME", "DELETE", "EXISTS", "FORMAT", "LIST_FILES", "INFO", "STAT", "MAKE_DIR", "SHA256"
        };
        const uint8_t op = static_cast<uint8_t>((fsBlocked >> 16) & 0xFFu);
        reboot["fsBlocked"] = op < 14 ? fsActions[op] : "UNKNOWN";
        reboot["fsBlockedWaitSec"] = static_cast<uint16_t>(fsBlocked & 0xFFFFu);
    }

    String out;
    out.reserve(measureJson(doc));
    serializeJson(doc, out);
    doc.clear();

    SyncFsImpl.writeFile(healthEventFileName, &out);
    log_info(F("Reboot health event saved to %s (reason=%s, marker=%s)"),
        healthEventFileName, resetReasonStr(resetReason), markerStr(resetMarker));
}
