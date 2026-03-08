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

void HealthMonitor::update(uint32_t timeoutMs) {
    const uint32_t nowMs = millis();
    bool allHealthy = true;
    
    if (nowMs - lastCheckInMs[0] > timeoutMs) { allHealthy = false; }
    if (nowMs - lastCheckInMs[1] > timeoutMs) { allHealthy = false; }
    if (nowMs - lastCheckInMs[2] > timeoutMs) { allHealthy = false; }

    if (allHealthy) {
        watchdog_update();
    } else {
        static uint32_t lastLogMs = 0;
        if (nowMs - lastLogMs > 1000) {
            log_warn(F("HealthMonitor: Task(s) unhealthy! [C0:%lu, C1:%lu, FX:%lu] now:%lu"), 
                lastCheckInMs[0], lastCheckInMs[1], lastCheckInMs[2], nowMs);
            lastLogMs = nowMs;
        }
    }
}
