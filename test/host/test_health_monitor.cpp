#include <cassert>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include <Arduino.h>
#include <hardware/watchdog.h>
#include "HealthMonitor.h"
#include "constants.hpp"
#include "log.h"

static void checkInCore0DuringMillis() {
    testMillisValue.fetch_add(1, std::memory_order_relaxed);
    HealthMonitor::checkIn(HEALTH_CORE0);
}

int main() {
    testMillisValue.store(1000, std::memory_order_relaxed);
    for (auto & scratch : watchdog_hw->scratch) scratch = 0xA5A5A5A5;
    HealthMonitor::checkIn(HEALTH_CORE0);
    HealthMonitor::update();
    assert(watchdog_hw->scratch[kCore0HeartbeatScratchIndex] == 0xA5A5A5A5);
    assert(testWatchdogUpdates.load(std::memory_order_relaxed) == 0);

    HealthMonitor::init();
    const uint32_t initialWarnings = test_log::warningCount.load(std::memory_order_relaxed);

    // The peer check-in happens after the caller sampled millis() but before it
    // reads peer timestamps, matching the reported unsigned-underflow race.
    testMillisValue.store(1500, std::memory_order_relaxed);
    testMillisHook = checkInCore0DuringMillis;
    HealthMonitor::checkIn(HEALTH_CORE1);
    assert(test_log::warningCount.load(std::memory_order_relaxed) == initialWarnings);

    // A genuinely stale task must still be reported.
    testMillisValue.store(4000, std::memory_order_relaxed);
    HealthMonitor::checkIn(HEALTH_CORE0);
    assert(test_log::warningCount.load(std::memory_order_relaxed) == initialWarnings + 1);
    {
        const std::lock_guard<std::mutex> lock(test_log::messageMutex);
        assert(test_log::lastWarning.find("FX:3000") != std::string::npos);
    }

    // Timestamp subtraction remains valid when millis() wraps around.
    testMillisValue.store(0xFFFFFFF0u, std::memory_order_relaxed);
    HealthMonitor::init();
    const uint32_t warningsBeforeRollover = test_log::warningCount.load(std::memory_order_relaxed);
    testMillisValue.store(0x20u, std::memory_order_relaxed);
    HealthMonitor::checkIn(HEALTH_CORE1);
    HealthMonitor::update(100, 60);
    assert(testWatchdogUpdates.load(std::memory_order_relaxed) == 1);
    assert(test_log::warningCount.load(std::memory_order_relaxed) == warningsBeforeRollover);

    // Competing task check-ins must claim the shared warning interval once.
    testMillisValue.store(20000, std::memory_order_relaxed);
    HealthMonitor::init();
    testMillisValue.store(25000, std::memory_order_relaxed);
    const uint32_t warningsBeforeConcurrentCheckIns = test_log::warningCount.load(std::memory_order_relaxed);
    std::atomic<bool> start{false};
    std::thread core0([&start] {
        while (!start.load(std::memory_order_acquire)) {}
        HealthMonitor::checkIn(HEALTH_CORE0);
    });
    std::thread core1([&start] {
        while (!start.load(std::memory_order_acquire)) {}
        HealthMonitor::checkIn(HEALTH_CORE1);
    });
    start.store(true, std::memory_order_release);
    core0.join();
    core1.join();
    assert(test_log::warningCount.load(std::memory_order_relaxed) == warningsBeforeConcurrentCheckIns + 1);
    return 0;
}