// MIT License
//
// Copyright (c) 2026 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef RP2350_LIGHTFX_LOGPICO_H
#define RP2350_LIGHTFX_LOGPICO_H

#include <Arduino.h>
#include <util/circular_buffer.h>

#define CR "\n"
#define PICO_LOG_VERSION_STR "1.0.0"
#define LOG_BUFFER_SIZE 10240

enum LogLevel:uint8_t {SILENT, FATAL, ERROR, WARNING, INFO, DEBUG, TRACE};

class PicoLog {
  public:
    PicoLog() = default;
    ~PicoLog() = default;

    void begin(SerialUSB* serial, LogLevel level = INFO);

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
    bool isEnabled(const LogLevel level) const { return level <= m_level && isStreamingEnabled(); }
    size_t write(LogLevel level, const char *data);
    size_t write(LogLevel level, const char *data, size_t len);
    size_t write(LogLevel level, const __FlashStringHelper *data);
    size_t getMinBufferSpace() const { return LOG_BUFFER_SIZE - m_maxBufferSize; }

    template<class T> size_t log(LogLevel level, const T format, ...) {
        if (!isEnabled(level)) return 0;
        va_list args;
        va_start(args, format);
        const size_t sz = print(level, format, args);
        va_end(args);
        return sz;
    }
    template<class T, typename... Args> size_t silent(const T format, Args... args) { return log(SILENT, format, args...); }
    template<class T, typename... Args> size_t fatal(const T format, Args... args) { return log(FATAL, format, args...); }
    template<class T, typename... Args> size_t error(const T format, Args... args) { return log(ERROR, format, args...); }
    template<class T, typename... Args> size_t warn(const T format, Args... args) { return log(WARNING, format, args...); }
    template<class T, typename... Args> size_t info(const T format, Args... args) { return log(INFO, format, args...); }
    template<class T, typename... Args> size_t debug(const T format, Args... args) { return log(DEBUG, format, args...); }
    template<class T, typename... Args> size_t trace(const T format, Args... args) { return log(TRACE, format, args...); }


private:
    LogLevel m_level{SILENT};
    LogUtil::CircularBuffer<char> m_queue{LOG_BUFFER_SIZE};
    Print* m_stream{nullptr};
    time_t m_timebase{0};
    size_t m_maxBufferSize{0};

    [[nodiscard]] bool isStreamingEnabled() const { return m_stream != nullptr; };
    size_t writeRaw(LogLevel level, const char *data, size_t len);
    size_t printTimestamp(char *msg, size_t capacity, time_t millisValue) const;
    size_t print(LogLevel level, const char* format, va_list args);
    size_t print(LogLevel level, const __FlashStringHelper *format, va_list args);
    static size_t printThread(char *msg, size_t capacity, const TaskStatus_t &taskStatus);
    static size_t printLevel(LogLevel level, char *msg, size_t capacity);
    friend void flushData();
};

extern PicoLog Log;

#endif //RP2350_LIGHTFX_LOGPICO_H