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

/**
 * Log severity levels in increasing order of verbosity.
 */
enum LogLevel:uint8_t {
    SILENT,     ///< Logging completely disabled
    FATAL,      ///< Unrecoverable fatal system errors
    ERROR,      ///< Error conditions requiring attention
    WARNING,    ///< Non-critical warnings
    INFO,       ///< Informational operational messages
    DEBUG,      ///< Diagnostic debug messages
    TRACE       ///< Fine-grained execution trace messages
};

/**
 * PicoLog RAM log buffer. Plain old data implementation to allow location independent memory mapping.
 * Circular buffer primitives to allow for efficient log message rotation.
 * Supports surviving warm reboot cycles.
 */
struct RamLogBuffer {
    uint32_t magic;         ///< Validation marker (RAM_LOG_MAGIC) to verify integrity after reset
    uint32_t head;          ///< Circular write index in data array
    uint32_t tail;          ///< Circular read index in data array
    uint32_t count;         ///< Number of valid log bytes currently stored
    uint32_t totalWritten;  ///< Monotonic cumulative byte counter across buffer lifetime
    char data[LOG_BUFFER_SIZE]; ///< Backing byte storage array

    /**
     * Validates magic sentinel and circular index consistency.
     * @return true if buffer is structurally valid and survived warm reset intact
     */
    [[nodiscard]] bool isValid() const {
        if (magic != RAM_LOG_MAGIC) return false;
        if (head >= LOG_BUFFER_SIZE || tail >= LOG_BUFFER_SIZE) return false;
        if (count > LOG_BUFFER_SIZE) return false;
        const uint32_t calcCount = (head >= tail) ? (head - tail) : (LOG_BUFFER_SIZE + head - tail);
        if (count != LOG_BUFFER_SIZE && count != calcCount) return false;
        return true;
    }

    /**
     * Initializes buffer indices, byte counters, and sets magic validation marker.
     */
    void init() {
        magic = RAM_LOG_MAGIC;
        head = 0;
        tail = 0;
        count = 0;
        totalWritten = 0;
    }

    /**
     * Appends bytes into the circular buffer, advancing head and overwriting oldest data when full.
     * @param src source byte array
     * @param len number of bytes to write
     */
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

    /**
     * Reads sequential log bytes from specified logical offset without advancing buffer pointers.
     * @param offset logical byte offset from tail
     * @param dest destination buffer
     * @param maxLen maximum number of bytes to read
     * @return number of bytes successfully read
     */
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

    /**
     * Extracts all valid log data in chronological order into destination String.
     * @param dest String to append log data into
     */
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

    /**
     * Clears buffer content and resets indices.
     */
    void clear() {
        init();
    }
};  // RamLogBuffer

/**
 * FreeRTOS-safe multi-core logger with RAM circular buffering and asynchronous Serial streaming.
 */
class PicoLog {
  public:
    PicoLog();
    ~PicoLog() = default;

    /**
     * Initializes logger mutex if not already initialized.
     */
    void init();

    /**
     * Configures the logger with an optional Serial stream and default log level.
     * Spawns asynchronous serial streaming task if serial stream is active.
     * @param serial pointer to SerialUSB interface (can be nullptr for RAM-only logging)
     * @param level initial log level threshold (defaults to INFO)
     */
    void begin(SerialUSB* serial, LogLevel level = INFO);

    /**
     * Flushes unstreamed log messages from circular RAM buffer to Serial output.
     */
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

    /**
     * Sets active logging level filter.
     * @param level maximum severity level to output
     */
    void setLevel(const LogLevel level) { m_level = level; };

    /**
     * Retrieves active logging level filter.
     * @return current LogLevel
     */
    LogLevel getLevel() const { return m_level; }

    /**
     * Checks if messages at given level are enabled by the current filter.
     * @param level level to test
     * @return true if enabled, false otherwise
     */
    bool isEnabled(const LogLevel level) const { return level <= m_level; }

    /**
     * Writes null-terminated log message to RAM buffer.
     * @param level message log level
     * @param data null-terminated string
     * @return bytes written
     */
    size_t write(LogLevel level, const char *data);

    /**
     * Writes log message buffer slice to RAM buffer.
     * @param level message log level
     * @param data pointer to message bytes
     * @param len byte count
     * @return bytes written
     */
    size_t write(LogLevel level, const char *data, size_t len);

    /**
     * Writes flash-resident log string to RAM buffer.
     * @param level message log level
     * @param data Flash string pointer
     * @return bytes written
     */
    size_t write(LogLevel level, const __FlashStringHelper *data);

    /**
     * Returns remaining unallocated byte space in circular RAM buffer.
     * @return free bytes before oldest records are overwritten
     */
    size_t getMinFreeSpace() const;

    /**
     * Checks whether valid crash log history from prior reset exists in RAM buffer.
     * @return true if valid unconsumed crash log is present
     */
    bool hasValidCrashLog() const;

    /**
     * Extracts surviving crash log into destination string and re-initializes buffer.
     * @param dest String to receive extracted crash log content
     */
    void snapshotCrashLog(String &dest);

    /**
     * Re-initializes RAM log buffer and resets serial streaming read position.
     */
    void resetBuffer();

    /**
     * Clears RAM log buffer (alias for resetBuffer()).
     */
    void clearBuffer();

    /**
     * Returns number of valid log bytes currently stored in RAM buffer.
     * @return byte count
     */
    size_t getBufferSize() const;

    /**
     * Reads a chunk of log bytes from the buffer at the given logical offset.
     * @param offset byte offset from tail
     * @param dest destination character buffer
     * @param maxLen maximum bytes to read
     * @return bytes read
     */
    size_t readBuffer(size_t offset, char *dest, size_t maxLen) const;

    /**
     * Extracts entire active log buffer in chronological order into a String.
     * @param dest String to receive log contents
     */
    void extractLog(String &dest) const;

    /**
     * Formats and writes log entry with timestamp, core ID, thread name, priority, and level.
     * @tparam T format string type (const char* or const __FlashStringHelper*)
     * @param level message log level
     * @param format printf-style format string
     * @param ... variable format arguments
     * @return total bytes written
     */
    template<class T> size_t log(LogLevel level, const T format, ...) {
        if (!isEnabled(level)) return 0;
        va_list args;
        va_start(args, format);
        const size_t sz = print(level, format, args);
        va_end(args);
        return sz;
    }

    /** Formatted logging at SILENT level. */
    template<class T, typename... Args> size_t silent(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(SILENT,  format, args...);
    }
    /** Formatted logging at FATAL level. */
    template<class T, typename... Args> size_t fatal(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(FATAL,   format, args...);
    }
    /** Formatted logging at ERROR level. */
    template<class T, typename... Args> size_t error(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(ERROR,   format, args...);
    }
    /** Formatted logging at WARNING level. */
    template<class T, typename... Args> size_t warn(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(WARNING, format, args...);
    }
    /** Formatted logging at INFO level. */
    template<class T, typename... Args> size_t info(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(INFO,    format, args...);
    }
    /** Formatted logging at DEBUG level. */
    template<class T, typename... Args> size_t debug(const T format, Args... args) {
        static_assert((std::is_trivially_copyable_v<Args> && ...), "Log arguments must be trivially copyable — use .c_str() for String/std::string");
        return log(DEBUG,   format, args...);
    }
    /** Formatted logging at TRACE level. */
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