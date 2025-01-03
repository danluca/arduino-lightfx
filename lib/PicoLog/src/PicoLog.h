// Copyright (c) 2024 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef PICOLOG_H
#define PICOLOG_H

#include <Arduino.h>
#include <util/circular_buffer.h>

#ifndef LOGGING_ENABLED
#define LOGGING_ENABLED 0
#endif

#define CR "\n"
#define PICO_LOG_VERSION_STR "1.0.0"
#define LOG_BUFFER_SIZE 8192

enum LogLevel:uint8_t {SILENT, FATAL, ERROR, WARNING, INFO, DEBUG, TRACE};

class PicoLog {
  public:
    PicoLog() : m_queue(LOG_BUFFER_SIZE) {};
    ~PicoLog() = default;

    void begin(SerialUSB* serial, LogLevel level = INFO);
    void setTimebase(time_t time);
    time_t getTimebase() const { return m_timebase; }
    void setLevel(const LogLevel level) { m_level = level; };
    LogLevel getLevel() const { return m_level; }
    bool isEnabled(const LogLevel level) const { return level <= m_level && isStreamingEnabled(); }
    size_t getMinBufferSpace() const { return LOG_BUFFER_SIZE - m_maxBufferSize; }

    template<class T> size_t log(LogLevel level, const T format, ...) {
#if LOGGING_ENABLED == 1
        if (!isEnabled(level)) return 0;
        va_list args;
        va_start(args, format);
        const size_t sz = print(level, format, args);
        va_end(args);
        return sz;
#else
        return 0;
#endif
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
    LogUtil::CircularBuffer<char> m_queue;
    Print* m_stream{nullptr};
    time_t m_timebase{0};
    size_t m_maxBufferSize{0};

    [[nodiscard]] bool isStreamingEnabled() const { return m_stream != nullptr; };
    size_t printTimestamp(char *msg) const;
    size_t print(LogLevel level, const char* format, va_list args);
    size_t print(LogLevel level, const __FlashStringHelper *format, va_list args);
    static size_t printThread(char *msg) ;
    static size_t printLevel(LogLevel level, char *msg) ;

    friend void flushData();
};

extern PicoLog Log;

#endif //PICOLOG_H
