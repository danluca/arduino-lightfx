// Copyright (c) by Dan Luca. All rights reserved.
//

#include "logPico.h"
#include "SchedulerExt.h"

#define SECS_PER_MIN  ((time_t)(60UL))
#define SECS_PER_HOUR ((time_t)(3600UL))
#define SECS_PER_DAY  ((time_t)(SECS_PER_HOUR * 24UL))

// log buffer stored in uninitialized RAM to facilitate log survival after warm boot
__attribute__((section(".uninitialized_ram"))) static RamLogBuffer s_ramLog;

PicoLog Log;
TaskWrapper *twStream;

static constexpr size_t RAW_WRITE_CHUNK_SIZE = 512;
static constexpr char fmtTimestamp[] PROGMEM = "%02lu:%02lu:%02lu.%03lu";
static constexpr char fmtTaskPriorityChanged[] PROGMEM = " [C%u-%s-%lu/%lu]";
static constexpr char fmtTaskPriorityRegular[] PROGMEM = " [C%u-%s-%lu]";
static constexpr char logLevelTags[] PROGMEM = "SFEWIDT";    //NOTE this string must be as long as LogLevel enum!
static constexpr char fmtLevel[] PROGMEM = " %c: ";

PicoLog::PicoLog() {
    mutex_init(&m_mutex);
    m_initialized = true;
}

void PicoLog::init() {
    if (!m_initialized) {
        mutex_init(&m_mutex);
        m_initialized = true;
    }
}

/**
 * Flushes newly written log bytes to the Serial stream without erasing the in-memory circular buffer.
 * If the log queue has no new bytes, delays execution briefly.
 */
void PicoLog::flush() {
    if (!isStreamingEnabled()) {
        vTaskDelay(pdMS_TO_TICKS(250));
        return;
    }

    char buf[RAW_WRITE_CHUNK_SIZE];
    size_t toWrite = 0;
    {
        CoreMutex coreMutex(&m_mutex);
        if (s_ramLog.isValid() && s_ramLog.count > 0) {
            const uint32_t total = s_ramLog.totalWritten;
            const uint32_t count = s_ramLog.count;
            if (m_serialReadPos + count < total) {
                m_serialReadPos = total - count;
            }
            if (m_serialReadPos < total) {
                const uint32_t unstreamed = total - m_serialReadPos;
                const size_t offset = static_cast<size_t>(m_serialReadPos - (total - count));
                const size_t requested = (unstreamed < RAW_WRITE_CHUNK_SIZE) ? static_cast<size_t>(unstreamed) : RAW_WRITE_CHUNK_SIZE;
                toWrite = s_ramLog.read(offset, buf, requested);
                m_serialReadPos += static_cast<uint32_t>(toWrite);
            }
        }
    }

    if (toWrite == 0) {
        vTaskDelay(pdMS_TO_TICKS(250));
        return;
    }
    m_stream->write(buf, toWrite);
    m_stream->flush();
}

void flushData() {
    Log.flush();
}

TaskDef tdStream {.setup = nullptr, .loop = flushData, .stackSize = 1024, .threadName = "SRL", .priority = TaskPriorityPolicy::SerialLogger, .core = CORE_ALL};

/**
 * Configures the logger using the Serial object. The Serial object can be null, in which case the logger
 * logs exclusively to the in-memory circular buffer.
 * @param serial pointer to the Serial object
 * @param level logging level
 */
void PicoLog::begin(SerialUSB *serial, const LogLevel level) {
    init();
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
 * Gets the amount of free space in the log buffer
 * @return amount of free space in the log buffer
 */
size_t PicoLog::getMinFreeSpace() const {
    CoreMutex coreMutex(&m_mutex);
    if (!s_ramLog.isValid()) return LOG_BUFFER_SIZE;
    return (s_ramLog.count >= LOG_BUFFER_SIZE) ? 0 : (LOG_BUFFER_SIZE - s_ramLog.count);
}

/**
 * Checks whether there is a valid crash log in the log buffer
 * @return whether there is a valid crash log in the log buffer
 */
bool PicoLog::hasValidCrashLog() const {
    CoreMutex coreMutex(&m_mutex);
    return s_ramLog.isValid() && s_ramLog.count > 0;
}

/**
 * Snapshot the crash log into the provided string
 * @param dest where to store the crash log
 */
void PicoLog::snapshotCrashLog(String &dest) {
    CoreMutex coreMutex(&m_mutex);
    if (s_ramLog.isValid()) {
        s_ramLog.extract(dest);
    }
    s_ramLog.init();
    m_serialReadPos = 0;
}

/**
 * Re-initialize the log buffer. Clears the log buffer and resets the serial read position.
 */
void PicoLog::resetBuffer() {
    CoreMutex coreMutex(&m_mutex);
    s_ramLog.init();
    m_serialReadPos = 0;
}

/**
 * Clears the log buffer. Same effect as resetBuffer().
 */
void PicoLog::clearBuffer() {
    CoreMutex coreMutex(&m_mutex);
    s_ramLog.clear();
    m_serialReadPos = 0;
}

/**
 * Get the size of the log buffer.
 * @return size of the log buffer
 */
size_t PicoLog::getBufferSize() const {
    CoreMutex coreMutex(&m_mutex);
    return s_ramLog.isValid() ? s_ramLog.count : 0;
}

/**
 * Read data from the log buffer.
 * @param offset offset from the beginning of the log buffer
 * @param dest destination buffer
 * @param maxLen maximum number of bytes to read
 * @return number of bytes read
 */
size_t PicoLog::readBuffer(const size_t offset, char *dest, const size_t maxLen) const {
    CoreMutex coreMutex(&m_mutex);
    return s_ramLog.read(offset, dest, maxLen);
}

/**
 * Extract the log data from the buffer and store it in a String.
 * @param dest String to store the extracted log data
 */
void PicoLog::extractLog(String &dest) const {
    CoreMutex coreMutex(&m_mutex);
    s_ramLog.extract(dest);
}

/**
 * Write a log message to the buffer. Length is determined by strlen(data).
 * @param level log level
 * @param data log message
 * @return number of bytes written
 */
size_t PicoLog::write(const LogLevel level, const char *data) {
    if (!isEnabled(level) || data == nullptr)
        return 0;
    return write(level, data, strlen(data));
}

/**
 * Write a log message to the buffer.
 * @param level log level
 * @param data log message
 * @param len length of the log message
 * @return number of bytes written
 */
size_t PicoLog::write(const LogLevel level, const char *data, const size_t len) {
    if (!isEnabled(level) || data == nullptr || len == 0)
        return 0;
    return writeRaw(level, data, len);
}

/**
 * Write a log message to the buffer.
 * @param level log level
 * @param data log message
 * @return number of bytes written
 */
size_t PicoLog::write(const LogLevel level, const __FlashStringHelper *data) {
    if (!isEnabled(level) || data == nullptr)
        return 0;
    const String raw(data);
    return writeRaw(level, raw.c_str(), raw.length());
}

/**
 * Write raw data to the log buffer.
 * @param level log level
 * @param data data to write
 * @param len number of bytes to write
 * @return number of bytes written
 */
size_t PicoLog::writeRaw(const LogLevel level, const char *data, const size_t len) {
    if (!isEnabled(level) || data == nullptr || len == 0)
        return 0;

#if LOG_BYPASS_BUFFER
    if (isStreamingEnabled())
        m_stream->write(reinterpret_cast<const uint8_t *>(data), len);
#endif

    CoreMutex coreMutex(&m_mutex);
    s_ramLog.write(data, len);
    return len;
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

    buf[sz] = '\n'; //new line ending - no null terminator as we control exactly the number of characters written into the circular buffer

#if LOG_BYPASS_BUFFER
    if (isStreamingEnabled())
        m_stream->write(buf, sz + 1);
#endif

    {
        CoreMutex coreMutex(&m_mutex);
        s_ramLog.write(buf, sz + 1);
    }

    if (heapUsed) delete[] buf;

    return sz;
}

/**
 * Appends current timestamp into the provided char array.
 * @param msg char array to append timestamp to
 * @param capacity capacity of the msg buffer
 * @param millisValue timestamp value in milliseconds
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
 * @param msg string to append thread info to
 * @param capacity capacity of msg buffer
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
 * @param level logging level to print
 * @param msg string to append level information to
 * @param capacity capacity of msg buffer
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

