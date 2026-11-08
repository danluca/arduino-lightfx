#pragma once

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace test_log {
inline std::atomic<uint32_t> warningCount{0};
inline std::atomic<uint32_t> errorCount{0};
inline std::mutex messageMutex;
inline std::string lastWarning;

inline void warn(const char *format, ...) {
    char message[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    {
        const std::lock_guard<std::mutex> lock(messageMutex);
        lastWarning = message;
    }
    warningCount.fetch_add(1, std::memory_order_relaxed);
}

inline void error(const char *, ...) {
    errorCount.fetch_add(1, std::memory_order_relaxed);
}
}

#define log_warn(...) test_log::warn(__VA_ARGS__)
#define log_error(...) test_log::error(__VA_ARGS__)