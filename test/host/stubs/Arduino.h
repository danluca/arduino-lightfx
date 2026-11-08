#pragma once

#include <atomic>
#include <cstdint>

inline std::atomic<uint32_t> testMillisValue{0};
inline void (*testMillisHook)() = nullptr;

inline uint32_t millis() {
    const uint32_t nowMs = testMillisValue.load(std::memory_order_relaxed);
    if (const auto hook = testMillisHook) {
        testMillisHook = nullptr;
        hook();
    }
    return nowMs;
}

#ifndef F
#define F(value) value
#endif