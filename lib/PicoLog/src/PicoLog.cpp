// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//

#include "logPico.h"
#include "SchedulerExt.h"

#define SECS_PER_MIN  ((time_t)(60UL))
#define SECS_PER_HOUR ((time_t)(3600UL))
#define SECS_PER_DAY  ((time_t)(SECS_PER_HOUR * 24UL))

PicoLog Log;
TaskWrapper *twStream;

#define SERIAL_BUFFER_SIZE 256
static constexpr char fmtTimestamp[] PROGMEM = "%02lu:%02lu:%02lu.%03lu";
static constexpr char fmtTaskPriorityChanged[] PROGMEM = " [C%u-%s-%lu/%lu]";
static constexpr char fmtTaskPriorityRegular[] PROGMEM = " [C%u-%s-%lu]";
static constexpr char logLevelTags[] PROGMEM = "SFEWIDT";    //NOTE this string must be as long as LogLevel enum!
static constexpr char fmtLevel[] PROGMEM = " %c: ";
static constexpr size_t RAW_WRITE_CHUNK_SIZE = 512;

/**
 * Flushes the log data queue by processing and outputting all queued log messages.
 * If the log queue is empty, delays execution briefly to allow for log gathering.
 */
void flushData() {
    if (Log.m_queue.empty()) {
        vTaskDelay(pdMS_TO_TICKS(250)); //empty log queue, allow some time to collect log statements
        return;
    }
    if (const size_t logSize = Log.m_queue.size(); logSize > Log.m_maxBufferSize)
        Log.m_maxBufferSize = logSize;
    while (!Log.m_queue.empty()) {
        char buf[SERIAL_BUFFER_SIZE]{0};    //zero-initialized buffer
        const size_t sz = min(Log.m_queue.size(), static_cast<size_t>(SERIAL_BUFFER_SIZE - 1));    //leave room for null terminator
        Log.m_queue.pop_front(buf, sz);
        buf[sz] = '\0'; //null-terminate for safety (not strictly required since we control write length)
        Log.m_stream->write(buf, sz);
    }
    Log.m_stream->flush();
}

TaskDef tdStream {nullptr, flushData, 1024, "SRL", 255, CORE_ALL};


/**
 * Configures the logger using the Serial object. The Serial objects can be null, in which case the logger will be disabled.
 * If needed to be enabled, this function must be called *after* Serial initialization
 * @param serial pointer to the Serial object
 * @param level logging level
 */
void PicoLog::begin(SerialUSB *serial, const LogLevel level) {
    m_level = level;
    if (serial && (*serial))
        m_stream = serial;

    if (isStreamingEnabled()) {
        log(INFO, F("Serial logging started at level %hu."), m_level);
        twStream = Scheduler.startTask(&tdStream);
        log(INFO, F("Serial logging thread [%s] - priority %u - has been setup id %u."), twStream->getName(), uxTaskPriorityGet(twStream->getTaskHandle()), twStream->getUID());
    }
}

size_t PicoLog::write(const LogLevel level, const char *data) {
    if (!isEnabled(level) || data == nullptr)
        return 0;
    return write(level, data, strlen(data));
}

size_t PicoLog::write(const LogLevel level, const char *data, const size_t len) {
    if (!isEnabled(level) || data == nullptr || len == 0)
        return 0;
    return writeRaw(level, data, len);
}

size_t PicoLog::write(const LogLevel level, const __FlashStringHelper *data) {
    if (!isEnabled(level) || data == nullptr)
        return 0;
    const String raw(data);
    return writeRaw(level, raw.c_str(), raw.length());
}

size_t PicoLog::writeRaw(const LogLevel level, const char *data, const size_t len) {
    if (!isEnabled(level) || data == nullptr || len == 0)
        return 0;

#if LOG_BYPASS_BUFFER
    if (!isStreamingEnabled()) return 0;
    m_stream->write(reinterpret_cast<const uint8_t *>(data), len);
    return len;
#else
    size_t written = 0;
    while (written < len) {
        const size_t chunk = min(len - written, RAW_WRITE_CHUNK_SIZE);
        m_queue.push_back(data + written, chunk);
        written += chunk;
    }
    return written;
#endif
}

/**
 * Helper method to print arguments using message patterns stored in flash
 * @param level logging level
 * @param format message pattern
 * @param args objects to replace the placeholders in the pattern message, in the order listed
 * @return size of the string written
 */
size_t PicoLog::print(const LogLevel level, const __FlashStringHelper *format, va_list args) {
    const String fStr(format);
    return print(level, fStr.c_str(), args);
}

/**
 * Writes the message with placeholders resolved from the args into a string
 * @param level logging level
 * @param format message pattern
 * @param args objects to replace the placeholders, in the order listed
 * @return size of the string written
 */
size_t PicoLog::print(const LogLevel level, const char *format, va_list args) {
    // Compute size using a copy of args to avoid consuming the original list
    va_list argsCopy;
    va_copy(argsCopy, args);
    const int payloadLen = vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);
    if (payloadLen < 0)
        return 0;
    const auto szMsg = static_cast<size_t>(payloadLen);

    TaskStatus_t taskStatus;
    vTaskGetInfo(nullptr, &taskStatus, pdFALSE, eRunning);
    const time_t msecs = millis() + m_timebase;
    const size_t szTimestamp = printTimestamp(nullptr, 0, msecs);
    const size_t szThread = printThread(nullptr, 0, taskStatus);
    const size_t szLevel = printLevel(level, nullptr, 0);

    // Prepare buffer: small messages on stack, large on heap to avoid large stack frames
    constexpr size_t STACK_CAP = 256;   // conservative stack allocation limit
    const size_t headerSize = szTimestamp + szThread + szLevel;
    const size_t needed = headerSize + szMsg + 1; // payload + newline

    char stackBuf[STACK_CAP];
    const bool heapUsed = needed > STACK_CAP;
    char *buf = heapUsed ? new char[needed] : stackBuf;

    const size_t capacity = heapUsed ? needed : STACK_CAP;
    size_t sz = printTimestamp(buf, capacity, msecs);
    sz += printThread(buf + sz, capacity - sz, taskStatus);
    sz += printLevel(level, buf + sz, capacity - sz);

    // Format the payload; if the buffer is smaller than needed, truncate safely
    const size_t payloadCap = capacity - sz;
    // Use a fresh copy of args for formatting to be safe
    va_list argsFormat;
    va_copy(argsFormat, args);
    const int wr = vsnprintf(buf + sz, payloadCap, format, argsFormat);  //at most payloadCap-1 bytes are written; terminated with null character
    va_end(argsFormat);
    sz += wr < 0 ? 0 : (static_cast<size_t>(wr) >= payloadCap ? payloadCap-1 : wr);

    // Ensure we don't write past our buffer; clamp sz to actual capacity
    if (const size_t maxWritable = (heapUsed ? needed : STACK_CAP) - 1; sz > maxWritable) sz = maxWritable;

    buf[sz] = '\n'; //new line ending - no null terminator as we control exactly the number of characters written into the m_queue or output stream

#if LOG_BYPASS_BUFFER
    if (isStreamingEnabled())
        m_stream->write(buf, sz + 1);
#else
    m_queue.push_back(buf, sz + 1);
#endif

    if (heapUsed) delete[] buf;

    return sz;
}

/**
 * Appends current timestamp into the provided char array.
 * NOTE: the provided char buffer must have space for ~20 chars for the timestamp. Caller's responsibility.
 * WARNING: risk of buffer overflow, no checks are made for whether writing the timestamp goes beyond the char array boundaries
 * This is acceptable as this is a private method, solely invoked from another private method \code print\endcode. While not ideal,
 * keeps the code simpler by avoiding checks and passing size arguments.
 * @param msg char array to append timestamp to
 * @return size of the data appended
 */
size_t PicoLog::printTimestamp(char *msg, const size_t capacity, const time_t millisValue) const {
    // Division constants
    constexpr unsigned long MSECS_PER_SEC = 1000;

    // Total time
    const unsigned long secs = millisValue / MSECS_PER_SEC;

    // Time in components
    const unsigned long MilliSeconds = millisValue % MSECS_PER_SEC;
    const unsigned long Seconds = secs % SECS_PER_MIN;
    const unsigned long Minutes = (secs / SECS_PER_MIN) % SECS_PER_MIN;
    const unsigned long Hours = (secs % SECS_PER_DAY) / SECS_PER_HOUR;

    if (msg == nullptr) {
        const int sz = snprintf(nullptr, 0, fmtTimestamp, Hours, Minutes, Seconds, MilliSeconds);
        return sz < 0 ? 0 : static_cast<size_t>(sz);
    }
    if (capacity == 0)
        return 0;

    const int sz = snprintf(msg, capacity, fmtTimestamp, Hours, Minutes, Seconds, MilliSeconds);
    return sz < 0 ? 0 : static_cast<size_t>(sz) >= capacity ? capacity - 1 : static_cast<size_t>(sz);
}

/**
 * Appends the current thread information into the provided string
 * NOTE: the provided char buffer must have space for ~20 chars for the thread info. Caller's responsibility.
 * WARNING: risk of buffer overflow, no checks are made for whether writing the thread info goes beyond the char array boundaries
 * This is acceptable as this is a private method, solely invoked from another private method \code print\endcode. While not ideal,
 * keeps the code simpler by avoiding checks and passing size arguments.
 * @param msg string to append thread info to
 * @param taskStatus the task status already populated
 * @return size of data appended
 */
size_t PicoLog::printThread(char *msg, const size_t capacity, const TaskStatus_t &taskStatus) {
    const uint coreNumber = get_core_num();
    if (msg == nullptr) {
        const int sz = taskStatus.uxCurrentPriority == taskStatus.uxBasePriority
            ? snprintf(nullptr, 0, fmtTaskPriorityRegular, coreNumber, taskStatus.pcTaskName, taskStatus.uxCurrentPriority)
            : snprintf(nullptr, 0, fmtTaskPriorityChanged, coreNumber, taskStatus.pcTaskName, taskStatus.uxCurrentPriority, taskStatus.uxBasePriority);
        return sz < 0 ? 0 : static_cast<size_t>(sz);
    }
    if (capacity == 0)
        return 0;
    if (taskStatus.uxCurrentPriority == taskStatus.uxBasePriority) {
        const int wr = snprintf(msg, capacity, fmtTaskPriorityRegular, coreNumber, taskStatus.pcTaskName, taskStatus.uxCurrentPriority);
        return wr < 0 ? 0 : static_cast<size_t>(wr) >= capacity ? capacity - 1 : static_cast<size_t>(wr);
    }
    const int wr = snprintf(msg, capacity, fmtTaskPriorityChanged, coreNumber, taskStatus.pcTaskName, taskStatus.uxCurrentPriority, taskStatus.uxBasePriority);
    return wr < 0 ? 0 : static_cast<size_t>(wr) >= capacity ? capacity - 1 : static_cast<size_t>(wr);
}

/**
 * Appends the logging level designation to the string provided
 * NOTE: the provided char buffer must have space for ~4 chars for the log level. Caller's responsibility.
 * WARNING: risk of buffer overflow, no checks are made for whether writing the log level goes beyond the char array boundaries
 * This is acceptable as this is a private method, solely invoked from another private method \code print\endcode. While not ideal,
 * keeps the code simpler by avoiding checks and passing size arguments.
 * @param level logging level to print
 * @param msg string to append level information to
 * @return size of data appended
 */
size_t PicoLog::printLevel(const LogLevel level, char *msg, const size_t capacity) {
    // Show log description based on log level
    const char *cLevel = logLevelTags + level;
    if (msg == nullptr) {
        const int sz = snprintf(nullptr, 0, fmtLevel, *cLevel);
        return sz < 0 ? 0 : static_cast<size_t>(sz);
    }
    if (capacity == 0)
        return 0;
    const int sz = snprintf(msg, capacity, fmtLevel, *cLevel);
    return sz < 0 ? 0 : static_cast<size_t>(sz) >= capacity ? capacity - 1 : static_cast<size_t>(sz);
}

/**
 * Sets a time offset to add to current {@code millis()} value (time since boot) in order to have calendar timestamps in the log statements
 * @param time the timebase value to set
 */
void PicoLog::setTimebase(const time_t time) {
    m_timebase = time;
}

