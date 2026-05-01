// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//
#include "HealthMonitor.h"
#include <hardware/watchdog.h>
#include "constants.hpp"
#include "global.h"
#include "log.h"
#include "sysinfo.h"

std::atomic<uint32_t> HealthMonitor::lastCheckInMs[3] = {0, 0, 0};
std::atomic<uint32_t> HealthMonitor::healthStatus = 0;
static constexpr uint16_t warnIntervalMs = 1000;
static constexpr uint16_t watchdogLowWatermarkMs = 2000;
static uint32_t lastWarnMs = 0;

void HealthMonitor::init() {
    const uint32_t nowMs = millis();
    for (auto & lastCheckIn : lastCheckInMs) lastCheckIn.store(nowMs, std::memory_order_relaxed);
    healthStatus.store(0xFF, std::memory_order_relaxed);
}

void HealthMonitor::checkIn(const HealthBit bit) {
    if (healthStatus.load(std::memory_order_relaxed) == 0)
        return;
    const uint32_t nowMs = millis();
    switch (bit) {
        case HEALTH_CORE0: lastCheckInMs[0].store(nowMs, std::memory_order_relaxed); watchdog_hw->scratch[kCore0HeartbeatScratchIndex] = nowMs; break;
        case HEALTH_CORE1: lastCheckInMs[1].store(nowMs, std::memory_order_relaxed); watchdog_hw->scratch[kCore1HeartbeatScratchIndex] = nowMs; break;
        case HEALTH_FX:    lastCheckInMs[2].store(nowMs, std::memory_order_relaxed); watchdog_hw->scratch[kFxHeartbeatScratchIndex] = nowMs; break;
        default: break;
    }
    uint32_t diffs[3];
    for (int i = 0; i < 3; i++) {
        diffs[i] = nowMs - lastCheckInMs[i].load(std::memory_order_relaxed);
    }

    if (const uint32_t wdleft = watchdog_get_time_remaining_ms(); wdleft < watchdogLowWatermarkMs) {
        // static uint32_t lastWarnMs = 0;
        if (nowMs - lastWarnMs > warnIntervalMs) {
#if LOGGING_ENABLED == 1
            log_warn(F("HealthMonitor-C: Task(s) slow! [C0:%lu, C1:%lu, FX:%lu] now:%lu, watchdog remaining: %lu"),
                diffs[0], diffs[1], diffs[2], nowMs, wdleft);
            logTaskStats();
#endif
            lastWarnMs = nowMs;
            saveSlownessHealthEvent(diffs[0], diffs[1], diffs[2], false);
        }
    }

}

void HealthMonitor::update(const uint32_t timeoutMs, const uint32_t warnMs) {
    if (healthStatus.load(std::memory_order_relaxed) == 0)
        return;
    const uint32_t nowMs = millis();
    bool allHealthy = true;
    uint32_t diffs[3];
    
    for (int i = 0; i < 3; i++) {
        // in odd race conditions, the check-in time may be in the future from nowMs (1ms observed)
        const uint32_t checkInMs = lastCheckInMs[i].load(std::memory_order_relaxed);
        diffs[i] = qsuba(nowMs, checkInMs);
        // ignore CORE1 checkin, its normal operation is to be blocked until a queue message arrives
        // ignore CORE0 checkin, it can block for long times during wifi reconnect, ping, etc.
        if (diffs[i] > timeoutMs && i == 2) {
            allHealthy = false;
        }
    }

    if (diffs[0] > 15000) {
        log_error(F("HealthMonitor-U: Persistent CORE0 starvation detected [C0:%lu] - triggering watchdog reset"), diffs[0]);
        watchdog_hw->scratch[kResetMarkerScratchIndex] = kResetMarkerCore0Stall;
        allHealthy = false;
    }

    if (allHealthy) {
        watchdog_update();
        if (diffs[0] > warnMs || diffs[1] > warnMs || diffs[2] > warnMs) {
            // static uint32_t lastWarnMs = 0;
            if (nowMs - lastWarnMs > warnIntervalMs) {
#if LOGGING_ENABLED == 1
                log_warn(F("HealthMonitor-U: Task(s) slow! [C0:%lu, C1:%lu, FX:%lu] now:%lu"),
                    diffs[0], diffs[1], diffs[2], nowMs);
                logTaskStats();
#endif
                lastWarnMs = nowMs;
                //saveSlownessHealthEvent(diffs[0], diffs[1], diffs[2], false);
            }
        }
    } else {
        // static uint32_t lastLogMs = 0;
        if (nowMs - lastWarnMs > warnIntervalMs) {
#if LOGGING_ENABLED == 1
            log_error(F("HealthMonitor-U: Task(s) STALLED! STOPS PINGING WATCHDOG. [C0:%lu, C1:%lu, FX:%lu] now:%lu, watchdog remaining: %lu"),
                diffs[0], diffs[1], diffs[2], nowMs, watchdog_get_time_remaining_ms());
            logTaskStats();
#endif
            lastWarnMs = nowMs;
            saveSlownessHealthEvent(diffs[0], diffs[1], diffs[2], true);
        }
    }
}
