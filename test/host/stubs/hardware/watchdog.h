#pragma once

#include <atomic>
#include <cstdint>

struct watchdog_hw_t {
    volatile uint32_t scratch[8]{};
};

inline watchdog_hw_t testWatchdogHardware{};
inline watchdog_hw_t *watchdog_hw = &testWatchdogHardware;
inline std::atomic<uint32_t> testWatchdogUpdates{0};

inline void watchdog_update() {
    testWatchdogUpdates.fetch_add(1, std::memory_order_relaxed);
}