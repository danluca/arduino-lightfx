// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//
#pragma once

#include <Arduino.h>
#include <atomic>

/**
 * Health bits for different tasks/cores
 */
enum HealthBit : uint32_t {
    HEALTH_CORE0 = 1 << 0,
    HEALTH_CORE1 = 1 << 1,
    HEALTH_FX    = 1 << 2,
    HEALTH_ALL   = HEALTH_CORE0 | HEALTH_CORE1 | HEALTH_FX
};

class HealthMonitor {
public:
    /**
     * Mark a task as healthy
     * @param bit the health bit to set
     */
    static void checkIn(HealthBit bit);

    /**
     * Perform the watchdog update if all tasks are healthy.
     * This should be called from one place (e.g. Core 0 loop)
     * @param timeoutMs the threshold in milliseconds for each health bit to stop feeding the watchdog (default 7000)
     * @param warnMs the threshold in milliseconds for logging a warning (default 3000)
     */
    static void update(uint32_t timeoutMs = 7000, uint32_t warnMs = 3000);

    /**
     * Initialize the health monitor
     */
    static void init();

private:
    static std::atomic<uint32_t> lastCheckInMs[3]; // For CORE0, CORE1, FX
    static std::atomic<uint32_t> healthStatus;
};
