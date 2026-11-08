// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//
#include "HealthMonitor.h"
#include <hardware/watchdog.h>
#include "constants.hpp"
#include "log.h"

std::atomic<uint32_t> HealthMonitor::lastCheckInMs[3] = {0, 0, 0};
std::atomic<bool> HealthMonitor::initialized{false};
static constexpr uint16_t warnIntervalMs = 1000;
static constexpr uint16_t watchdogLowWatermarkMs = 2000;
static std::atomic<uint32_t> lastCheckInWarnMs{0};

static bool claimCheckInWarning(uint32_t &nowMs) {
    uint32_t previousWarnMs = lastCheckInWarnMs.load(std::memory_order_relaxed);
    while (true) {
        nowMs = millis();
        if (nowMs - previousWarnMs <= warnIntervalMs)
            return false;
        if (lastCheckInWarnMs.compare_exchange_weak(previousWarnMs, nowMs, std::memory_order_relaxed, std::memory_order_relaxed))
            return true;
    }
}

void HealthMonitor::init() {
    const uint32_t nowMs = millis();
    for (auto & lastCheckIn : lastCheckInMs) lastCheckIn.store(nowMs, std::memory_order_relaxed);
    initialized.store(true, std::memory_order_release);
}

void HealthMonitor::checkIn(const HealthBit bit) {
    if (!initialized.load(std::memory_order_acquire))
        return;

    uint32_t lastCheckIns[3];
    for (int i = 0; i < 3; i++)
        lastCheckIns[i] = lastCheckInMs[i].load(std::memory_order_relaxed);

    const uint32_t nowMs = millis();
    switch (bit) {
        case HEALTH_CORE0: lastCheckInMs[0].store(nowMs, std::memory_order_relaxed); lastCheckIns[0] = nowMs; watchdog_hw->scratch[kCore0HeartbeatScratchIndex] = nowMs; break;
        case HEALTH_CORE1: lastCheckInMs[1].store(nowMs, std::memory_order_relaxed); lastCheckIns[1] = nowMs; watchdog_hw->scratch[kCore1HeartbeatScratchIndex] = nowMs; break;
        case HEALTH_FX:    lastCheckInMs[2].store(nowMs, std::memory_order_relaxed); lastCheckIns[2] = nowMs; watchdog_hw->scratch[kFxHeartbeatScratchIndex] = nowMs; break;
        default: break;
    }
    uint32_t diffs[3];

    for (int i = 0; i < 3; i++) {
        diffs[i] = nowMs - lastCheckIns[i];
    }

    //watchdog_get_time_remaining_ms() has a HW bug on RP2040, returns always 5
    if (diffs[0] > watchdogLowWatermarkMs || diffs[1] > watchdogLowWatermarkMs || diffs[2] > watchdogLowWatermarkMs) {
        uint32_t warningNowMs;
        if (claimCheckInWarning(warningNowMs)) {
            for (int i = 0; i < 3; i++) diffs[i] = warningNowMs - lastCheckIns[i];
            log_warn(F("HealthMonitor-C: Task(s) slow! [C0:%lu, C1:%lu, FX:%lu] now:%lu"),
                diffs[0], diffs[1], diffs[2], warningNowMs);
        }
    }
}

void HealthMonitor::update(const uint32_t timeoutMs, const uint32_t warnMs) {
    if (!initialized.load(std::memory_order_acquire))
        return;

    uint32_t lastCheckIns[3];
    for (int i = 0; i < 3; i++)
        lastCheckIns[i] = lastCheckInMs[i].load(std::memory_order_relaxed);

    const uint32_t nowMs = millis();
    bool allHealthy = true;
    uint32_t diffs[3];
    
    for (int i = 0; i < 3; i++) {
        diffs[i] = nowMs - lastCheckIns[i];
        // ignore CORE1 checkin, its normal operation is to be blocked until a queue message arrives
        // ignore CORE0 checkin, it can block for long times during wifi reconnect, ping, etc.
        if (diffs[i] > timeoutMs && i == 2) {
            allHealthy = false;
        }
    }

    if (allHealthy) {
        watchdog_update();
        if (diffs[0] > warnMs || diffs[1] > warnMs || diffs[2] > warnMs) {
            static uint32_t lastWarnMs = 0;
            if (nowMs - lastWarnMs > warnIntervalMs) {
                log_warn(F("HealthMonitor-U: Task(s) slow! [C0:%lu, C1:%lu, FX:%lu] now:%lu"),
                    diffs[0], diffs[1], diffs[2], nowMs);
                lastWarnMs = nowMs;
            }
        }
    } else {
        static uint32_t lastLogMs = 0;
        if (nowMs - lastLogMs > warnIntervalMs) {
            log_error(F("HealthMonitor-U: Task(s) STALLED! STOPS PINGING WATCHDOG. [C0:%lu, C1:%lu, FX:%lu] now:%lu"),
                diffs[0], diffs[1], diffs[2], nowMs);
            lastLogMs = nowMs;
        }
    }
}
