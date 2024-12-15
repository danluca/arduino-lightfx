// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include "PicoLog.h"
#include "../../SchedulerExt/src/SchedulerExt.h"

#define SECS_PER_MIN  ((time_t)(60UL))
#define SECS_PER_HOUR ((time_t)(3600UL))
#define SECS_PER_DAY  ((time_t)(SECS_PER_HOUR * 24UL))

PicoLog Log;
TaskWrapper *twStream;

/**
 * Flushes the log data queue by processing and outputting all queued log messages.
 * If the log queue is empty, delays execution briefly to allow for log gathering.
 */
void flushData() {
    if (Log.m_queue.empty()) {
        vTaskDelay(pdMS_TO_TICKS(100)); //empty log queue, allow some time to collect log statements
        return;
    }
    while (!Log.m_queue.empty()) {
        const String *msg = Log.m_queue.pop();
        Log.m_stream->println(msg->c_str());
        delete msg;
    }
}

TaskDef tdStream {nullptr, flushData, 640, "Ser", 255, CORE_ALL};

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
        twStream = Scheduler.startTask(&tdStream);
        log(INFO, F("Serial logging thread [%s] - priority %d - has been setup id %u."), twStream->getName(), uxTaskPriorityGet(twStream->getTaskHandle()), twStream->getUID());
    }
}

/**
 * Helper method to print arguments using message patterns stored in flash
 * @param level logging level
 * @param format message pattern
 * @param args objects to replace the placeholders in the pattern message, in the order listed
 * @return size of the string written
 */
size_t PicoLog::print(const LogLevel level, const __FlashStringHelper *format, const va_list args) {
    PGM_P p = reinterpret_cast<PGM_P>(format);
    return print(level, reinterpret_cast<const char *>(p), args);
}


/**
 * Writes the message with placeholders resolved from the args into a string
 * @param level logging level
 * @param format message pattern
 * @param args objects to replace the placeholders, in the order listed
 * @return size of the string written
 */
size_t PicoLog::print(const LogLevel level, const char *format, const va_list args) {
    auto *msg = new String();   // heap memory fragmentation risk; this instance is released when sent through Serial interface (flushData)
    size_t sz = printTimestamp(msg);
    sz += printThread(msg);
    sz += printLevel(level, msg);
    msg->reserve(vsprintf(nullptr, format, args) + sz);
    vsprintf(msg->end(), format, args);
    m_queue.push(msg);
    return msg->length();
}

/**
 * Appends current timestamp into the provided string
 * @param msg string to append timestamp to
 * @return size of the data appended
 */
size_t PicoLog::printTimestamp(String *msg) const {
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
    msg->reserve(msg->length() + 20);
    return sprintf(msg->end(), "%02lu:%02lu:%02lu.%03lu ", Hours, Minutes, Seconds, MilliSeconds);
}

/**
 * Appends the current thread information into the provided string
 * @param msg string to append thread info to
 * @return size of data appended
 */
size_t PicoLog::printThread(String *msg) {
    TaskStatus_t taskStatus;
    vTaskGetInfo(nullptr, &taskStatus, pdFALSE, eRunning);
    constexpr char fmt[] = "[%s-%u.%u] ";
    const size_t sz = sprintf(nullptr, fmt, taskStatus.pcTaskName, taskStatus.uxCurrentPriority, taskStatus.uxBasePriority);
    msg->reserve(msg->length() + sz);
    return sprintf(msg->end(), fmt, taskStatus.pcTaskName, taskStatus.uxCurrentPriority, taskStatus.uxBasePriority);
}

/**
 * Appends the logging level designation to the string provided
 * @param level logging level to print
 * @param msg string to append level information to
 * @return size of data appended
 */
size_t PicoLog::printLevel(const LogLevel level, String *msg) {
    // Show log description based on log level
    const size_t sz = msg->length();
    switch (level) {
        case SILENT: msg->concat("SLN: "); break;
        case FATAL: msg->concat("FTL: "); break;
        case ERROR: msg->concat("ERR: "); break;
        case WARNING: msg->concat("WRN: "); break;
        case INFO: msg->concat("INF: "); break;
        case DEBUG: msg->concat("DBG: "); break;
        case TRACE: msg->concat("TRC: "); break;
    }
    return msg->length() - sz;
}

/**
 * Sets a time offset to add to current {@code millis()} value (time since boot) in order to have calendar timestamps in the log statements
 * @param time the timebase value to set
 */
void PicoLog::setTimebase(const time_t time) {
    m_timebase = time;
}

