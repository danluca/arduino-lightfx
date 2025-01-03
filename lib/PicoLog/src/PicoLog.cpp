// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include "PicoLog.h"

#include <FS.h>

#include "../../SchedulerExt/src/SchedulerExt.h"

#define SECS_PER_MIN  ((time_t)(60UL))
#define SECS_PER_HOUR ((time_t)(3600UL))
#define SECS_PER_DAY  ((time_t)(SECS_PER_HOUR * 24UL))

PicoLog Log;
TaskWrapper *twStream;

#define SERIAL_BUFFER_SIZE 256
static constexpr char fmtTimestamp[] PROGMEM = "%02lu:%02lu:%02lu.%03lu";
static constexpr char fmtTaskPriorityChanged[] PROGMEM = " [%s-%u/%u]";
static constexpr char fmtTaskPriorityRegular[] PROGMEM = " [%s-%u]";
static constexpr char logLevelTags[] PROGMEM = "SFEWIDT";    //NOTE this string must be as long as LogLevel enum!
static constexpr char fmtLevel[] PROGMEM = " %c: ";

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
        const size_t sz = min(Log.m_queue.size(), SERIAL_BUFFER_SIZE);    //leave room for null terminator
        Log.m_queue.pop_front(buf, sz);
        buf[sz] = '\0'; //may not be needed, we're writing out the exact size of buffer read
        Log.m_stream->write(buf, sz);
    }
    Log.m_stream->flush();
}

TaskDef tdStream {nullptr, flushData, 640, "SRL", 255, CORE_ALL};


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
    const size_t szMsg = vsnprintf(nullptr, 0, format, args) + 1;
    char buf[szMsg+48]{};  //sufficient for the header, e.g. '00:00:05.338 [CORE0-5.5] I: ' - 28 chars
    size_t sz = printTimestamp(buf);
    sz += printThread(buf + sz);
    sz += printLevel(level, buf + sz);
    sz += vsnprintf(buf + sz, szMsg, format, args);
    buf[sz] = '\n'; //new line ending - no null terminator as we control exactly the number of characters written into the m_queue
    m_queue.push_back(buf, sz + 1);
    return sz;
}

/**
 * Appends current timestamp into the provided char array.
 * NOTE: the provided char buffer must have space for ~20 chars for the timestamp.
 * WARNING: risk of buffer overflow, no checks are made for whether writing the timestamp goes beyond the char array boundaries
 * @param msg char array to append timestamp to
 * @return size of the data appended
 */
size_t PicoLog::printTimestamp(char *msg) const {
    // Division constants
    constexpr unsigned long MSECS_PER_SEC = 1000;

    // Total time
    const time_t msecs = millis() + m_timebase;
    const unsigned long secs = msecs / MSECS_PER_SEC;

    // Time in components
    const unsigned long MilliSeconds = msecs % MSECS_PER_SEC;
    const unsigned long Seconds = secs % SECS_PER_MIN;
    const unsigned long Minutes = (secs / SECS_PER_MIN) % SECS_PER_MIN;
    const unsigned long Hours = (secs % SECS_PER_DAY) / SECS_PER_HOUR;

    // Time as string
    return snprintf(msg, 20, fmtTimestamp, Hours, Minutes, Seconds, MilliSeconds);
}

/**
 * Appends the current thread information into the provided string
 * NOTE: the provided char buffer must have space for ~20 chars for the thread info.
 * WARNING: risk of buffer overflow, no checks are made for whether writing the thread info goes beyond the char array boundaries
 * @param msg string to append thread info to
 * @return size of data appended
 */
size_t PicoLog::printThread(char *msg) {
    TaskStatus_t taskStatus;
    vTaskGetInfo(nullptr, &taskStatus, pdFALSE, eRunning);
    if (taskStatus.uxCurrentPriority == taskStatus.uxBasePriority) {
        const size_t sz = snprintf(nullptr, 0, fmtTaskPriorityRegular, taskStatus.pcTaskName, taskStatus.uxCurrentPriority);
        return snprintf(msg, sz + 1, fmtTaskPriorityRegular, taskStatus.pcTaskName, taskStatus.uxCurrentPriority);
    }
    const size_t sz = snprintf(nullptr, 0, fmtTaskPriorityChanged, taskStatus.pcTaskName, taskStatus.uxCurrentPriority, taskStatus.uxBasePriority);
    return snprintf(msg, sz + 1, fmtTaskPriorityChanged, taskStatus.pcTaskName, taskStatus.uxCurrentPriority, taskStatus.uxBasePriority);
}

/**
 * Appends the logging level designation to the string provided
 * NOTE: the provided char buffer must have space for ~4 chars for the log level.
 * WARNING: risk of buffer overflow, no checks are made for whether writing the log level goes beyond the char array boundaries
 * @param level logging level to print
 * @param msg string to append level information to
 * @return size of data appended
 */
size_t PicoLog::printLevel(const LogLevel level, char *msg) {
    // Show log description based on log level
    const char *cLevel = logLevelTags + level;
    return snprintf(msg, 5, fmtLevel, *cLevel);
}

/**
 * Sets a time offset to add to current {@code millis()} value (time since boot) in order to have calendar timestamps in the log statements
 * @param time the timebase value to set
 */
void PicoLog::setTimebase(const time_t time) {
    m_timebase = time;
}

