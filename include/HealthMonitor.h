// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//
#pragma once

#include <Arduino.h>
#include <atomic>

/**
 * Health bits for monitored tasks and CPU cores.
 */
enum HealthBit : uint32_t {
    HEALTH_CORE0 = 1 << 0,  ///< Core 0 main application and network loop heartbeat
    HEALTH_CORE1 = 1 << 1,  ///< Core 1 secondary worker task heartbeat
    HEALTH_FX    = 1 << 2,  ///< Core 1 FX LED rendering engine heartbeat
    HEALTH_ALL   = HEALTH_CORE0 | HEALTH_CORE1 | HEALTH_FX  ///< Mask of all monitored tasks
};

/**
 * Multi-core task liveness supervisor and hardware watchdog monitor.
 */
class HealthMonitor {
public:
    /**
     * Mark a task as healthy by recording its current check-in timestamp and updating watchdog scratch registers.
     * @param bit health bit identifying the task/core checking in
     */
    static void checkIn(HealthBit bit);

    /**
     * Evaluates task health and performs the watchdog update if all monitored tasks are healthy.
     * Detects task stalls and persistent Core 0 starvation, logging warnings and triggering resets if needed.
     * Should be called periodically from the Core 0 loop.
     * @param timeoutMs threshold in milliseconds for task check-ins before stopping watchdog feed (default 7000)
     * @param warnMs threshold in milliseconds for logging slowness warnings (default 3000)
     */
    static void update(uint32_t timeoutMs = 7000, uint32_t warnMs = 3000);

    /**
     * Initialize the health monitor and record baseline check-in timestamps.
     */
    static void init();

    /**
     * Enter OTA mode — extends the CORE0 starvation detection threshold to 120s while firmware upload is in progress.
     */
    static void enterOtaMode();

    /**
     * Exit OTA mode — restores normal CORE0 starvation detection threshold (15s).
     */
    static void exitOtaMode();

    /**
     * Returns true while a firmware upload is in progress (OTA mode is active).
     * @return true if in OTA mode, false otherwise
     */
    static bool isInOtaMode();

private:
    static std::atomic<uint32_t> lastCheckInMs[3]; ///< Check-in timestamps (ms) for CORE0, CORE1, FX
    static std::atomic<bool> initialized;          ///< True once health monitor initialized
    static std::atomic<bool> otaMode;              ///< True while OTA firmware upload active
};
