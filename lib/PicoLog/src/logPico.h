// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//
#pragma once
#ifndef RP2350_LIGHTFX_LOGPICO_H
#define RP2350_LIGHTFX_LOGPICO_H

#include <Arduino.h>
#include <atomic>
#include <type_traits>
#include <CoreMutex.h>

#define CR "\n"
#define PICO_LOG_VERSION_STR "1.1.0"
#define LOG_BUFFER_SIZE 32768
#define RAM_LOG_MAGIC 0x504C4F47u // "PLOG"

enum LogLevel:uint8_t {SILENT, FATAL, ERROR, WARNING, INFO, DEBUG, TRACE};

/**
 * PicoLog RAM log buffer. Plain old data implementation to allow location independent memory mapping.
 * Circular buffer primitives to allow for efficient log message rotation.
 * Supports surviving warm reboot cycles.
 */
struct RamLogBuffer {
    uint32_t magic;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t totalWritten;
    char data[LOG_BUFFER_SIZE];

    [[nodiscard]] bool isValid() const {
        if (magic != RAM_LOG_MAGIC) return false;
        if (head >= LOG_BUFFER_SIZE || tail >= LOG_BUFFER_SIZE) return false;
        if (count > LOG_BUFFER_SIZE) return false;
        const uint32_t calcCount = (head >= tail) ? (head - tail) : (LOG_BUFFER_SIZE + head - tail);
        if (count != LOG_BUFFER_SIZE && count != calcCount) return false;
        return true;
    }

    void init() {
        magic = RAM_LOG_MAGIC;
        head = 0;
        tail = 0;
        count = 0;
        totalWritten = 0;
    }

    void write(const char *src, const size_t len) {
        if (!isValid()) {
            init();
        }
        for (size_t i = 0; i < len; ++i) {
            data[head] = src[i];
            head = (head + 1) % LOG_BUFFER_SIZE;
            if (count < LOG_BUFFER_SIZE) {
                count++;
            } else {
                tail = (tail + 1) % LOG_BUFFER_SIZE;
            }
        }
        totalWritten += len;
    }

    size_t read(const size_t offset, char *dest, const size_t maxLen) const {
        if (!isValid() || offset >= count || dest == nullptr || maxLen == 0) return 0;
        const size_t avail = (count - offset) < maxLen ? (count - offset) : maxLen;
        size_t pos = (tail + offset) % LOG_BUFFER_SIZE;
        for (size_t i = 0; i < avail; ++i) {
            dest[i] = data[pos];
            pos = (pos + 1) % LOG_BUFFER_SIZE;
        }
        return avail;
    }

    void extract(String &dest) const {
        if (!isValid() || count == 0) return;
        dest.reserve(dest.length() + count);
        if (head > tail) {
            dest.concat(data + tail, count);
        } else {
            const size_t firstChunk = LOG_BUFFER_SIZE - tail;
            dest.concat(data + tail, firstChunk);
            if (head > 0) {
                dest.concat(data, head);
            }
        }
    }

    void clear() {
        init();
    }
};  // RamLogBuffer

class PicoLog {
  public:
    PicoLog();
    ~PicoLog() = default;

    void init();
    void begin(SerialUSB* serial, LogLevel level = INFO);
    void flush();

    /**
     * Sets the timebase used for logging operations - essentially an offset to be added to
     * the system's real time clock to get the current calendar time
     *
     * @param time The time value to set as the timebase in milliseconds
     */
    void setTimebase(time_t time);

    /**
     * Retrieves the current timebase used for logging operations.
     *
     * @return The timebase value in milliseconds.
     */
    time_t getTimebase() const { return m_timebase; }
    void setLevel(const LogLevel level) { m_level = level; };
    LogLevel getLevel() const { return m_level; }
    bool isEnabled(const LogLevel level) const { return level <= m_level; }
    size_t write(LogLevel level, const char *data);
    size_t write(LogLevel level, const char *data, size_t len);
    size_t write(LogLevel level, const __FlashStringHelper *data);
    size_t getMinFreeSpace() const;

    bool hasValidCrashLog() const;
    void snapshotCrashLog(String &dest);
    void resetBuffer();
    void clearBuffer();
    size_t getBufferSize() const;
    size_t readBuffer(size_t offset, char *dest, size_t maxLen) const;
    void extractLog(String &dest) const;

    template<class T> size_t log(LogLevel level, const T format, ...) {
        if (!isEnabled(level)) return 0;
        va_list args;
        va_start(args, format);
        const size_t sz = print(level, format, args);
        va_end(args);
        return sz;
    }
    template<class T, typename... Args> size_t silent(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(SILENT,  format, args...);
    }
    template<class T, typename... Args> size_t fatal(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(FATAL,   format, args...);
    }
    template<class T, typename... Args> size_t error(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(ERROR,   format, args...);
    }
    template<class T, typename... Args> size_t warn(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(WARNING, format, args...);
    }
    template<class T, typename... Args> size_t info(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(INFO,    format, args...);
    }
    template<class T, typename... Args> size_t debug(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(DEBUG,   format, args...);
    }
    template<class T, typename... Args> size_t trace(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(TRACE,   format, args...);
    }


private:
    LogLevel m_level{SILENT};
    Print* m_stream{nullptr};
    time_t m_timebase{0};
    uint32_t m_serialReadPos{0};
    mutable mutex_t m_mutex{};
    bool m_initialized{false};

    [[nodiscard]] bool isStreamingEnabled() const { return m_stream != nullptr; };
    size_t writeRaw(LogLevel level, const char *data, size_t len);
    size_t printTimestamp(char *msg, size_t capacity, time_t millisValue) const;
    size_t print(LogLevel level, const char* format, va_list args);
    size_t print(LogLevel level, const __FlashStringHelper *format, va_list args);
    static size_t printThread(char *msg, size_t capacity, const TaskStatus_t &taskStatus);
    static size_t printLevel(LogLevel level, char *msg, size_t capacity);
};

extern PicoLog Log;

#endif //RP2350_LIGHTFX_LOGPICO_H