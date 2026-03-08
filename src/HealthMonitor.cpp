// MIT License
//
// Copyright (c) 2026 by Dan Luca. All rights reserved.
//
#include "HealthMonitor.h"
#include <hardware/watchdog.h>
#include "constants.hpp"
#include "log.h"

uint32_t HealthMonitor::lastCheckInMs[3] = {0, 0, 0};
uint32_t HealthMonitor::healthStatus = 0;

void HealthMonitor::init() {
    const uint32_t nowMs = millis();
    for (uint i = 0; i < 3; i++) lastCheckInMs[i] = nowMs;
    healthStatus = 0;
}

void HealthMonitor::checkIn(HealthBit bit) {
    const uint32_t nowMs = millis();
    switch (bit) {
        case HEALTH_CORE0: lastCheckInMs[0] = nowMs; watchdog_hw->scratch[kCore0HeartbeatScratchIndex] = nowMs; break;
        case HEALTH_CORE1: lastCheckInMs[1] = nowMs; watchdog_hw->scratch[kCore1HeartbeatScratchIndex] = nowMs; break;
        case HEALTH_FX:    lastCheckInMs[2] = nowMs; watchdog_hw->scratch[kFxHeartbeatScratchIndex] = nowMs; break;
        default: break;
    }
}

void HealthMonitor::update(uint32_t timeoutMs, uint32_t warnMs) {
    const uint32_t nowMs = millis();
    bool allHealthy = true;
    uint32_t diffs[3];
    
    for (int i = 0; i < 3; i++) {
        diffs[i] = nowMs - lastCheckInMs[i];
        if (diffs[i] > timeoutMs) {
            allHealthy = false;
        }
    }

    if (allHealthy) {
        watchdog_update();
        if (diffs[0] > warnMs || diffs[1] > warnMs || diffs[2] > warnMs) {
            static uint32_t lastWarnMs = 0;
            if (nowMs - lastWarnMs > 1000) {
                log_warn(F("HealthMonitor: Task(s) slow! [C0:%lu, C1:%lu, FX:%lu] now:%lu"), 
                    diffs[0], diffs[1], diffs[2], nowMs);
                lastWarnMs = nowMs;
            }
        }
    } else {
        static uint32_t lastLogMs = 0;
        if (nowMs - lastLogMs > 1000) {
            log_error(F("HealthMonitor: Task(s) STALLED! STOPS PINGING WATCHDOG. [C0:%lu, C1:%lu, FX:%lu] now:%lu"), 
                diffs[0], diffs[1], diffs[2], nowMs);
            lastLogMs = nowMs;
        }
    }
}
