// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SchedulerExt.h>
#include <FastLED.h>
#include <atomic>
#include "hardware/watchdog.h"
#include "filesystem.h"
#include "util.h"
#include "sysinfo.h"
#include "config.h"
#include "timeutil.h"
#include "version.h"
#include "constants.hpp"
#include "log.h"
#if LOGGING_ENABLED == 1
#include <stringutils.h>
#endif

#define BUF_ID_SIZE  20

#if LOGGING_ENABLED == 1
// static constexpr char threadInfoFmt[] = "[%u] %s:: time=%s [%u%%] priority(c.b)=%u.%u state=%s id=%u core=%#X stackSize=%u free=%u\n";
static constexpr auto heapStackInfoFmt = "HEAP/STACK INFO\n  Stack     :: ptr=%#X;\n  Heap      :: size=%zu used=%zu free=%zu lowest=%zu block max/min/free=%zu/%zu/%zu\n";
static constexpr auto heapPSRAMInfoFmt = "  PSRAM Heap:: PSRAM=%zu size=%d (free=%d used=%d)\n";
static constexpr auto sysInfoFmt = "SYSTEM INFO\n  CPU ROM %d [%.1f MHz] CORE %d\n  FreeRTOS version %s\n  Arduino PICO version %s [SDK %s]\n  Board UID 0x%s name '%s'\n  MAC Address %s\n  Device name %s build version %s at %s\n  Flash size %u";
static constexpr auto fmtTaskInfo = "%-10s\t%s\t%u%c\t%-6u  %-4u\t0x%02x  %-12llu  %.2f %%\n";
static constexpr auto fmtTotalCPULoad = "\nTotal CPU Load:    %.2f %% / %.2f s\n";
#endif
static constexpr auto unknown = "N/A";
static constexpr auto idleTaskMarker = "idle";
constexpr CRGB CLR_ALL_OK = CRGB::Indigo;
constexpr CRGB CLR_SETUP_IN_PROGRESS = CRGB::Green;
constexpr CRGB CLR_UPGRADE_PROGRESS = CRGB::Blue;
constexpr CRGB CLR_SETUP_ERROR = CRGB::Red;

unsigned long prevStatTime = 0;
unsigned long prevIdleTime = 0;
SysInfo *sysInfo;

static bool sysInfoDirty = true;                       // mark dirty at startup so first flush happens
static uint32_t sysInfoLastSaveMs = 0;
static constexpr uint32_t sysInfoSaveIntervalMs = 300u * 1000u; // 300 seconds
static std::atomic<bool> sysInfoSaveInProgress(false);

namespace {

constexpr UBaseType_t kTaskSnapshotSlack = 2;
constexpr unsigned long kTaskSnapshotIntervalMs = 5000ul;
constexpr unsigned long kTaskSnapshotMaxAgeMs = kTaskSnapshotIntervalMs * 2;
constexpr unsigned long kTaskSnapshotForceMinIntervalMs = 10000ul;

/**
 * Represents a snapshot of task runtime statistics and system resource usage at a specific point in time.
 *
 * This structure encapsulates detailed information about FreeRTOS tasks, system run time, CPU usage,
 * heap statistics, and the timestamp of when the snapshot was captured. It is primarily used for
 * diagnostics, performance analysis, and system monitoring.
 *
 * Key fields include:
 * - `tasks`: A collection of task status data retrieved using FreeRTOS API functions. Each entry provides
 *   task-specific details such as runtime metrics, priority, and stack usage.
 * - `sysTotalRunTimeRaw`: Raw runtime counter value provided by FreeRTOS, representing the total execution
 *   cycles since boot.
 * - `totalRunTime`: Aggregated runtime for all tasks in microseconds, representing workload distribution.
 * - `idleRunTime`: Recorded runtime for the idle task, which can be used to calculate CPU usage.
 * - `heapStats`: Comprehensive statistics about heap memory usage, obtained using `vPortGetHeapStats`.
 * - `capturedAtMs`: Timestamp in milliseconds (using `millis()` function) when the snapshot was taken.
 * - `valid`: Boolean flag indicating whether the snapshot contains valid data. Useful for error handling
 *   in diagnostic routines.
 *
 * Usage:
 * TaskRuntimeSnapshot is passed between diagnostic functions to compute metrics such as CPU load percentage,
 * snapshot window duration, and per-task runtime summaries.
 *
 * Notes:
 * - Memory held by the `tasks` vector may be dynamically allocated/cleared, depending on the FreeRTOS task
 *   count at the time of snapshot.
 * - The `sysTotalRunTimeRaw` and `totalRunTime` fields facilitate calculation of CPU usage percentages and
 *   activity breakdowns.
 * - The structure is critical for functions like `logTaskStats` and `taskStats` to log or serialize the data.
 */
struct TaskRuntimeSnapshot {
    std::vector<TaskStatus_t> tasks{};
    configRUN_TIME_COUNTER_TYPE sysTotalRunTimeRaw{0};
    uint64_t totalRunTime{0};
    uint64_t idleRunTime{0};
    HeapStats_t heapStats{};
    unsigned long capturedAtMs{0};
    bool valid{false};
};

/**
 * Maintains a historical record of task runtime snapshots for diagnostics and analysis.
 *
 * This structure is designed to encapsulate and manage records of task runtime data captured
 * at various points in time. It provides mechanisms for thread-safe access and capturing
 * of snapshots to facilitate performance monitoring, workload analysis, and system diagnostics.
 *
 * Key fields include:
 * - `stateMutex`: A mutex to synchronize access to the state of the runtime history, ensuring that
 *   concurrent operations on the data do not conflict.
 * - `captureMutex`: A mutex to guard the actual snapshot-capturing process, avoiding conflicts
 *   during collection of runtime statistics.
 * - `current`: A TaskRuntimeSnapshot structure representing the most recent snapshot of task runtime
 *   statistics and system resource usage.
 * - `previous`: A TaskRuntimeSnapshot structure representing the preceding snapshot of task runtime
 *   statistics. Provides historical context for comparisons.
 * - `lastForcedCaptureMs`: The timestamp, in milliseconds, of the last explicitly forced snapshot capture.
 *   Useful for enforcing snapshot capture intervals.
 *
 * Usage:
 * TaskRuntimeHistory is utilized by system functions that require both current and historical
 * runtime data to compute metrics such as task activity trends, system load variations, and task
 * behavior over time. It ensures safe updates and access to runtime history in a multithreaded
 * environment.
 *
 * Notes:
 * - Both `stateMutex` and `captureMutex` employ mutual exclusion to protect data consistency
 *   in concurrent environments. They handle access to the `current` and `previous` snapshots
 *   separately for independent operations.
 * - The `lastForcedCaptureMs` field is particularly important in scenarios where forced captures
 *   (bypassing regular capture intervals) need to be logged and enforced for periodic monitoring.
 * - This structure is vital for functions like `captureTaskRuntimeSnapshot` and
 *   `copyTaskRuntimeHistory`, which depend on time-series data to perform effective diagnostics
 *   and runtime analysis.
 */
struct TaskRuntimeHistory {
    mutable mutex_t stateMutex{};
    mutable mutex_t captureMutex{};
    TaskRuntimeSnapshot current{};
    TaskRuntimeSnapshot previous{};
    unsigned long lastForcedCaptureMs{0};
};

TaskRuntimeHistory g_taskRuntimeHistory{};

} // namespace

// constexpr TaskDef stLedTasks {nullptr, state_led_run, 384, "LED", 3, CORE_0};

const char *taskStatusToString(const eTaskState state) {
    switch (state) {
        case eReady: return csReady;
        case eBlocked: return csBlocked;
        case eSuspended: return csSuspended;
        case eDeleted: return csDeleted;
        case eRunning: return csRunning;
        case eInvalid: return csInvalid;
        default: return unknown;
    }
}

/**
 * Generic comparator of tasks by number - works with TaskStatus_t pointers
 * @param a TaskStatus_t pointer A to compare
 * @param b TaskStatus_t pointer B to compare
 * @return result of comparing task A number to task B number
 */
static int compareTasksByNumber(const void* a, const void* b) {
    const auto* taskA = static_cast<const TaskStatus_t*>(a);
    const auto* taskB = static_cast<const TaskStatus_t*>(b);
    return static_cast<int>(taskA->xTaskNumber) - static_cast<int>(taskB->xTaskNumber);
}

/**
 * Checks if task name is an idle task name - i.e. named IDLE0 or IDLE1
 * @param taskName task name to check
 * @return true if task name is an idle task name, false otherwise
 */
static bool isIdleTaskName(const char *taskName) {
    if (taskName == nullptr)
        return false;
    // idle tasks are named IDLE0 and IDLE1; the other IdleCore0 and IdleCore1 are not considered idle tasks - their purpose is to put the other core on idle
    return strstr(taskName, "IDLE") != nullptr;
}

/**
 * Finds task with matching number in task status collection
 * @param taskStatusArray task status collection
 * @param taskNumber the task number to find
 * @return task with matching number or nullptr
 */
static const TaskStatus_t *findTaskStatus(const std::vector<TaskStatus_t> &taskStatusArray, const UBaseType_t taskNumber) {
    for (const auto &taskStatus : taskStatusArray) {
        if (taskStatus.xTaskNumber == taskNumber)
            return &taskStatus;
    }
    return nullptr;
}

/**
 * Calculates the difference between two runtime values, handling wraparound
 * @param currentValue current runtime value
 * @param previousValue previous runtime value
 * @return runtime delta
 */
static uint64_t runtimeDelta(const uint64_t currentValue, const uint64_t previousValue) {
    return currentValue >= previousValue ? currentValue - previousValue : 0;
}

/**
 * Calculates the percentage of runtime value relative to total runtime
 * @param runtimeValue runtime value
 * @param totalRuntime total runtime
 * @return runtime percentage
 */
static float runtimePct(const uint64_t runtimeValue, const uint64_t totalRuntime) {
    return totalRuntime > 0 ? static_cast<float>(runtimeValue) * 100.0f / static_cast<float>(totalRuntime) : 0.0f;
}

/**
 * Calculates the CPU load percentage based on task runtime snapshots
 * @param current current task snapshot
 * @param previous previous task snapshot
 * @return CPU load percentage
 */
static float cpuLoadPct(const TaskRuntimeSnapshot &current, const TaskRuntimeSnapshot &previous) {
    const uint64_t totalDelta = runtimeDelta(current.totalRunTime, previous.totalRunTime);
    const uint64_t idleDelta = runtimeDelta(current.idleRunTime, previous.idleRunTime);
    return runtimePct(totalDelta > idleDelta ? totalDelta - idleDelta : 0, totalDelta);
}

/**
 * Calculates the time window in seconds between two task snapshots
 * @param current current task snapshot
 * @param previous previous task snapshot
 * @return time window in seconds
 */
static float snapshotWindowSec(const TaskRuntimeSnapshot &current, const TaskRuntimeSnapshot &previous) {
    return previous.valid
        ? static_cast<float>(current.capturedAtMs - previous.capturedAtMs) / 1000.0f
        : 0.0f;
}

/**
 * Checks if task runtime snapshot is fresh based on age - threshold configured by kTaskSnapshotMaxAgeMs
 * @param snapshot task runtime snapshot
 * @param nowMs current time in milliseconds
 * @return true if the snapshot is fresh, false otherwise
 */
static bool isSnapshotFresh(const TaskRuntimeSnapshot &snapshot, const unsigned long nowMs) {
    return snapshot.valid && (nowMs - snapshot.capturedAtMs) <= kTaskSnapshotMaxAgeMs;
}

/**
 * Populates task runtime snapshot by capturing task status and system total runtime
 * @param snapshot task runtime snapshot to populate
 * @return true if snapshot was successfully populated, false otherwise
 */
static bool populateTaskRuntimeSnapshot(TaskRuntimeSnapshot &snapshot) {
    snapshot = TaskRuntimeSnapshot{};

    for (uint8_t attempt = 0; attempt < 2; attempt++) {
        const UBaseType_t taskCount = uxTaskGetNumberOfTasks();
        if (taskCount == 0)
            return false;

        const UBaseType_t capacity = taskCount + kTaskSnapshotSlack;
        snapshot.tasks.assign(capacity, TaskStatus_t{});
        snapshot.sysTotalRunTimeRaw = 0;

        const UBaseType_t actualCount = uxTaskGetSystemState(snapshot.tasks.data(), capacity, &snapshot.sysTotalRunTimeRaw);
        if (actualCount == 0) {
            snapshot.tasks.clear();
            return false;
        }
        if (actualCount >= capacity && uxTaskGetNumberOfTasks() > capacity && attempt == 0)
            continue;

        snapshot.tasks.resize(actualCount);
        if (!snapshot.tasks.empty())
            qsort(snapshot.tasks.data(), actualCount, sizeof(TaskStatus_t), compareTasksByNumber);

        for (const auto &taskStatus : snapshot.tasks) {
            snapshot.totalRunTime += taskStatus.ulRunTimeCounter;
            if (isIdleTaskName(taskStatus.pcTaskName))
                snapshot.idleRunTime += taskStatus.ulRunTimeCounter;
        }

        vPortGetHeapStats(&snapshot.heapStats);
        snapshot.capturedAtMs = millis();
        snapshot.valid = true;
        return true;
    }

    snapshot.tasks.clear();
    return false;
}

/**
 * Copies task runtime history from current to previous snapshot
 * @param current current task runtime snapshot
 * @param previous previous task runtime snapshot
 * @return true if copy was successful, false otherwise
 */
static bool copyTaskRuntimeHistory(TaskRuntimeSnapshot &current, TaskRuntimeSnapshot &previous) {
    CoreMutex lock(&g_taskRuntimeHistory.stateMutex);
    if (!g_taskRuntimeHistory.current.valid)
        return false;

    current = g_taskRuntimeHistory.current;
    previous = g_taskRuntimeHistory.previous;
    return true;
}

/**
 * Captures task runtime snapshot, updating current and previous snapshots
 * @param force whether to force capture regardless of freshness
 * @return true if capture was successful, false otherwise
 */
bool captureTaskRuntimeSnapshot(const bool force) {
    const unsigned long nowMs = millis();
    if (force) {
        CoreMutex stateLock(&g_taskRuntimeHistory.stateMutex);
        if (isSnapshotFresh(g_taskRuntimeHistory.current, nowMs))
            return true;
        if (g_taskRuntimeHistory.current.valid &&
            g_taskRuntimeHistory.lastForcedCaptureMs != 0 &&
            (nowMs - g_taskRuntimeHistory.lastForcedCaptureMs) < kTaskSnapshotForceMinIntervalMs)
            return g_taskRuntimeHistory.current.valid;
        g_taskRuntimeHistory.lastForcedCaptureMs = nowMs;
    }

    CoreMutex captureLock(&g_taskRuntimeHistory.captureMutex);
    if (force) {
        TaskRuntimeSnapshot current;
        TaskRuntimeSnapshot previous;
        if (copyTaskRuntimeHistory(current, previous) && isSnapshotFresh(current, millis()))
            return true;
    }

    TaskRuntimeSnapshot snapshot;
    if (!populateTaskRuntimeSnapshot(snapshot))
        return false;

    CoreMutex stateLock(&g_taskRuntimeHistory.stateMutex);
    g_taskRuntimeHistory.previous = g_taskRuntimeHistory.current;
    g_taskRuntimeHistory.current = std::move(snapshot);
    if (force)
        g_taskRuntimeHistory.lastForcedCaptureMs = g_taskRuntimeHistory.current.capturedAtMs;
    return true;
}

/**
 * Loads task runtime history by copying current to previous snapshot and capturing if necessary
 * @param current current task runtime snapshot
 * @param previous previous task runtime snapshot
 * @return true if load was successful, false otherwise
 */
static bool loadTaskRuntimeHistory(TaskRuntimeSnapshot &current, TaskRuntimeSnapshot &previous) {
    if (copyTaskRuntimeHistory(current, previous) && isSnapshotFresh(current, millis()))
        return true;
    if (!captureTaskRuntimeSnapshot(true))
        return copyTaskRuntimeHistory(current, previous);
    return copyTaskRuntimeHistory(current, previous);
}

/**
 * Logs detailed information about FreeRTOS task statistics and heap usage.
 * This method retrieves and processes data on tasks and heap memory allocation,
 * providing valuable insights for debugging and performance optimization.
 *
 * The task information includes details such as the name of each task, state, priority,
 * runtime percentages, stack usage, core affinity, and runtime counters across cores.
 *
 * Heap statistics report total stack size, free stack space, heap size, and usage.
 *
 * Key actions:
 * 1. Captures the number of active FreeRTOS tasks and allocates memory for task status data.
 * 2. Obtains task runtime metrics, including runtime counters broken down by cores.
 * 3. Computes percentage runtime of individual tasks relative to the total runtime.
 * 4. Logs per-task details formatted as a readable string.
 * 5. Frees dynamically allocated memory for task status data.
 * 6. Reports task-related and heap details as formatted logs.
 *
 * Note: This method performs critical memory allocations such as `pvPortMalloc` for task data
 * and ensures proper cleanup using `vPortFree`. Care should be taken when modifying to avoid
 * memory leaks or division by zero errors.
 *
 * This function references:
 * - FreeRTOS API: `uxTaskGetSystemState`, `vTaskList`, `uxTaskGetNumberOfTasks`.
 * - Scheduler tasks: Uses `Scheduler.getTask` to get additional task-related metadata.
 * - Utility formatting: Employs utility functions like `StringUtils::append` for creating compact logs.
 *
 * This method is resource-intensive and should only be run in contexts where such
 * overhead does not impact the system performance significantly.
 */
void logTaskStats() {
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;
    TaskRuntimeSnapshot current;
    TaskRuntimeSnapshot previous;
    if (!loadTaskRuntimeHistory(current, previous))
        return;

    const uint64_t totalDelta = runtimeDelta(current.totalRunTime, previous.totalRunTime);
    String strTaskInfo;
    StringUtils::append(strTaskInfo, F("TASK STATS [sys total run time %llu, delta cycles %llu, current time %lu ms, %s\n  total CPU cycles 32/64bit %lu / %llu, total task cycles cur/prev %llu / %llu, CPU frequency %d Hz]\n"),
        current.sysTotalRunTimeRaw, totalDelta, current.capturedAtMs, TimeFormat::asStringMs(nowMillis()).c_str(),
        rp2040.getCycleCount(), rp2040.getCycleCount64(), current.totalRunTime, previous.totalRunTime, sysInfo->getCPUFrequency());
    log_info(F("%s"), strTaskInfo.c_str());
    log_write(INFO, F("Name      \tSt \tPr \tStk     Num \tCore  RunTime       RunPct\n"));

    for (const auto &taskStatus : current.tasks) {
        const TaskStatus_t *prevTaskStatus = findTaskStatus(previous.tasks, taskStatus.xTaskNumber);
        const uint64_t taskDeltaTime = prevTaskStatus != nullptr
            ? runtimeDelta(taskStatus.ulRunTimeCounter, prevTaskStatus->ulRunTimeCounter)
            : taskStatus.ulRunTimeCounter;
        const float taskPct = runtimePct(taskDeltaTime, totalDelta);
        const char prElevated = taskStatus.uxCurrentPriority > taskStatus.uxBasePriority
            ? '+'
            : taskStatus.uxCurrentPriority < taskStatus.uxBasePriority ? '-' : ' ';
        const uint coreAffinity = taskStatus.uxCoreAffinityMask >= CORE_ALL ? CORE_ALL : taskStatus.uxCoreAffinityMask;
        char buf[96];
        snprintf(buf, sizeof(buf), fmtTaskInfo, taskStatus.pcTaskName, taskStatusToString(taskStatus.eCurrentState),
            static_cast<uint>(taskStatus.uxCurrentPriority), prElevated, static_cast<uint>(taskStatus.usStackHighWaterMark),
            static_cast<uint>(taskStatus.xTaskNumber), coreAffinity, taskDeltaTime, taskPct);
        log_write(INFO, buf);
    }

    char buf[80];
    snprintf(buf, sizeof(buf), fmtTotalCPULoad, cpuLoadPct(current, previous), snapshotWindowSec(current, previous));
    log_write(INFO, buf);
    // Simple heap stats
    logHeapStats();
    struct mallinfo mf = mallinfo();
    log_info(F("Malloc memory stats: allocated=%u, used=%u, free=%u"), mf.arena, mf.uordblks, mf.fordblks);

    log_info(F("Minimum log buffer free space %zu bytes"), Log.getMinBufferSpace());
    // log_info(F("Current watchdog remaining value %u us"), watchdog_get_time_remaining_ms());

#endif
}

/**
 * Logs a system-wide summary of FreeRTOS task metrics, CPU usage, and heap statistics.
 * Called from CORE1 task (on Core 1) that runs diagnostics.
 *
 * This function calculates and logs key performance metrics, offering insights into
 * task execution and memory usage for real-time diagnostics and optimization. It is intended
 * to provide a compact summary of system activity and resource utilization over a defined
 * time window.
 *
 * Key features:
 * 1. Retrieves the total number of active FreeRTOS tasks.
 * 2. Allocates and processes task state and runtime data using `uxTaskGetSystemState`.
 * 3. Computes total runtime and idle runtime, identifying idle tasks by name.
 * 4. Calculates CPU load percentage based on changes in runtime metrics over time.
 * 5. Retrieves heap statistics such as total usage, free memory, minimum recorded free memory,
 *    number of free blocks, and size of the largest free block.
 * 6. Logs data in a concise format, including task count, CPU load percentage, heap usage, and
 *    key memory metrics.
 *
 * Implementation notes:
 * - Uses a static state for maintaining previous values of runtime counters and system time
 *   to compute time-windowed metrics.
 * - Allocates dynamic memory for capturing FreeRTOS task information, which is released
 *   after processing.
 * - Makes use of utility functions like `millis()` for timing and `log_info()` for message output.
 *
 * This method depends on:
 * - FreeRTOS APIs: `uxTaskGetSystemState`, `uxTaskGetNumberOfTasks`, `vPortGetHeapStats`.
 * - Task naming conventions: `isIdleTaskName` for detecting idle tasks.
 * - Logging framework: `Log` methods for conditional logging and message generation.
 *
 * Logging impact:
 * - Can be resource-intensive when run frequently, as it processes runtime information
 *   for all tasks and dynamically allocates memory. Should be used judiciously in performance-
 *   critical scenarios.
 */
void logTaskSummary() {
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;
    TaskRuntimeSnapshot current;
    TaskRuntimeSnapshot previous;
    if (!loadTaskRuntimeHistory(current, previous))
        return;

    log_info(F("TASK SUMMARY: tasks=%u cpuLoad=%.2f %% window=%.2f s heapUsed=%zu heapFree=%zu heapLow=%zu freeBlocks=%zu largestFree=%zu"),
        static_cast<unsigned>(current.tasks.size()),
        cpuLoadPct(current, previous),
        snapshotWindowSec(current, previous),
        configTOTAL_HEAP_SIZE - current.heapStats.xAvailableHeapSpaceInBytes,
        current.heapStats.xAvailableHeapSpaceInBytes,
        current.heapStats.xMinimumEverFreeBytesRemaining,
        current.heapStats.xNumberOfFreeBlocks,
        current.heapStats.xSizeOfLargestFreeBlockInBytes);
#endif
}

/**
 * Log Heap memory allocation stats
 */
void logHeapStats() {
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;    // Simple heap stats
    String strHeapInfo;
    strHeapInfo.reserve(256);  //ensure enough space to avoid reallocations
    HeapStats_t heapStats;
    vPortGetHeapStats(&heapStats);
    StringUtils::append(strHeapInfo, heapStackInfoFmt, rp2040.getStackPointer(), configTOTAL_HEAP_SIZE, (configTOTAL_HEAP_SIZE-heapStats.xAvailableHeapSpaceInBytes), heapStats.xAvailableHeapSpaceInBytes,
        heapStats.xMinimumEverFreeBytesRemaining, heapStats.xSizeOfLargestFreeBlockInBytes, heapStats.xSizeOfSmallestFreeBlockInBytes, heapStats.xNumberOfFreeBlocks);
#ifdef PICO_RP2350
    StringUtils::append(strHeapInfo, heapPSRAMInfoFmt, rp2040.getPSRAMSize(), rp2040.getTotalPSRAMHeap(), rp2040.getFreePSRAMHeap(), rp2040.getUsedPSRAMHeap());
#endif
    log_info(F("%s"), strHeapInfo.c_str());
#endif
}

/**
 * See resetReason_t enum definition in RP2040Support.h
 * @param reason resetReason_t numeric enum value of reset reason
 * @return string representation of the reset reason numeric value
 */
const char *resetReasonToString(const RP2040::resetReason_t reason) {
    switch (reason) {
        case RP2040::UNKNOWN_RESET: return unknown;
        case RP2040::PWRON_RESET: return csPowerOn;
        case RP2040::RUN_PIN_RESET: return csPinReset;
        case RP2040::SOFT_RESET: return csSoftReset;
        case RP2040::WDT_RESET: return csWatchdog;
        case RP2040::DEBUG_RESET: return csDebug;
        case RP2040::GLITCH_RESET: return csGlitch;
        case RP2040::BROWNOUT_RESET: return csBrownout;
        default: return unknown;
    }
}

const char *resetMarkerToString(const uint32_t marker) {
    switch (marker) {
        case kResetMarkerNone: return "none";
        case kResetMarkerPanic: return "panic";
        case kResetMarkerAssert: return "assert";
        case kResetMarkerHardFault: return "hardfault";
        case kResetMarkerMalloc: return "malloc_failed";
        case kResetMarkerStackOverflow: return "stack_overflow";
        case kResetMarkerFxStall: return "fx_stall";
        case kResetMarkerOta: return "ota";
        case kResetMarkerReboot: return "reboot";
        case kResetMarkerUnknown: return "unknown";
        default: return "other";
    }
}

const char *fxStageToString(const uint32_t stage) {
    switch (stage) {
        case kFxStageNone: return "none";
        case kFxStageEnter: return "enter";
        case kFxStageAfterQueue: return "after_queue";
        case kFxStageAfterOtaCheck: return "after_ota_check";
        case kFxStageFirmwareUpgrade: return "fw_upgrade";
        case kFxStageBeforeLoop: return "before_loop";
        case kFxStageAfterLoop: return "after_loop";
        case kFxStageAfterPing: return "after_ping";
        default: return "unknown";
    }
}

[[maybe_unused]] static const char *fsBlockedActionToString(const uint8_t action) {
    switch (action) {
        case 0: return "READ_FILE";
        case 1: return "WRITE_FILE";
        case 2: return "WRITE_FILE_ASYNC";
        case 3: return "APPEND_FILE";
        case 4: return "APPEND_FILE_BIN";
        case 5: return "RENAME";
        case 6: return "DELETE";
        case 7: return "EXISTS";
        case 8: return "FORMAT";
        case 9: return "LIST_FILES";
        case 10: return "INFO";
        case 11: return "STAT";
        case 12: return "MAKE_DIR";
        case 13: return "SHA256";
        default: return "UNKNOWN";
    }
}

/**
 * Logs detailed system information for debugging and diagnostic purposes.
 * Called from CORE0 task.
 * This function outputs various system-level details, including:
 * - CPU ROM version and core speed
 * - FreeRTOS, Arduino PICO, and SDK version details
 * - Board identifier, board name, and MAC address
 * - Device name and flash memory size
 * - System reset reason with detailed status codes
 */
void logSystemInfo() {
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;
    log_info(sysInfoFmt, rp2040_rom_version(), RP2040::f_cpu()/1000000.0, RP2040::cpuid(), tskKERNEL_VERSION_NUMBER, ARDUINO_PICO_VERSION_STR, PICO_SDK_VERSION_STRING,
               sysInfo->getBoardId().c_str(), BOARD_NAME, sysInfo->getMacAddress().c_str(), DEVICE_NAME, sysInfo->getBuildVersion().c_str(), sysInfo->getBuildTime().c_str(),
               sysInfo->get_flash_capacity());
    log_info(F("System reset reason %s"), resetReasonToString(rp2040.getResetReason()));
    const uint32_t resetMarker = watchdog_hw->scratch[kResetMarkerScratchIndex];
    log_info(F("System reset marker %s (0x%08lX)"), resetMarkerToString(resetMarker), resetMarker);
    watchdog_hw->scratch[kResetMarkerScratchIndex] = kResetMarkerNone;
    const uint32_t fxHeartbeat = watchdog_hw->scratch[kFxHeartbeatScratchIndex];
    log_info(F("FX heartbeat marker 0x%08lX"), fxHeartbeat);
    watchdog_hw->scratch[kFxHeartbeatScratchIndex] = 0u;
    const uint32_t fxStage = watchdog_hw->scratch[kFxStageScratchIndex];
    log_info(F("FX stage marker %s (0x%08lX)"), fxStageToString(fxStage), fxStage);
    watchdog_hw->scratch[kFxStageScratchIndex] = kFxStageNone;
#if DIAG_CORE_HEARTBEATS
    const uint32_t core0Heartbeat = watchdog_hw->scratch[kCore0HeartbeatScratchIndex];
    log_info(F("CORE0 heartbeat marker 0x%08lX"), core0Heartbeat);
    watchdog_hw->scratch[kCore0HeartbeatScratchIndex] = 0u;
    const uint32_t core1Heartbeat = watchdog_hw->scratch[kCore1HeartbeatScratchIndex];
    log_info(F("CORE1 heartbeat marker 0x%08lX"), core1Heartbeat);
    watchdog_hw->scratch[kCore1HeartbeatScratchIndex] = 0u;
#endif
    const uint32_t fsBlocked = watchdog_hw->scratch[kFsBlockedScratchIndex];
    if ((fsBlocked & 0xFF000000u) == kFsBlockedMagic) {
        const uint8_t op = static_cast<uint8_t>((fsBlocked >> 16) & 0xFFu);
        const uint16_t waitedSeconds = static_cast<uint16_t>(fsBlocked & 0xFFFFu);
        log_info(F("FS blocked marker op=%s (%u) waited=%u sec"), fsBlockedActionToString(op), op, waitedSeconds);
    } else if (fsBlocked != 0u) {
        log_info(F("FS blocked marker raw 0x%08lX"), fsBlocked);
    }
    watchdog_hw->scratch[kFsBlockedScratchIndex] = 0u;

    //interesting memory pointers from pico-sdk/src/rp2_common/pico_crt0/rp2040/memmap_default.ld
    extern char __exidx_start;
    extern char __exidx_end;
    extern char __etext;
    extern char __data_start__;
    extern char __preinit_array_start;
    extern char __preinit_array_end;
    extern char __init_array_start;
    extern char __init_array_end;
    extern char __fini_array_start;
    extern char __fini_array_end;
    extern char __data_end__;
    extern char __bss_start__;
    extern char __bss_end__;
    extern char __end__;
    extern char __HeapLimit;
    extern char __StackLimit;
    extern char __StackTop;
    extern char __StackBottom;
    extern char __StackOneTop;
    extern char __StackOneBottom;
    extern uint32_t __scratch_x_source__;
    extern uint32_t __scratch_y_source__;
    log_info(F("Memory map pointers:"));
    log_info(F("  .text end:            __etext       = %#X"), (uint32_t)&__etext);
    log_info(F("  .data start/end:      __data_start__/__data_end__ = %#X/%#X"), (uint32_t)&__data_start__, (uint32_t)&__data_end__);
    log_info(F("  .bss start/end:       __bss_start__/__bss_end__   = %#X/%#X"), (uint32_t)&__bss_start__, (uint32_t)&__bss_end__);
    log_info(F("  .exidx start/end:     __exidx_start__/__exidx_end__ = %#X/%#X"), (uint32_t)&__exidx_start, (uint32_t)&__exidx_end);
    log_info(F("  .preinit_array start/end: __preinit_array_start__/__preinit_array_end__ = %#X/%#X"), (uint32_t)&__preinit_array_start, (uint32_t)&__preinit_array_end);
    log_info(F("  .init_array start/end:    __init_array_start__/__init_array_end__     = %#X/%#X"), (uint32_t)&__init_array_start, (uint32_t)&__init_array_end);
    log_info(F("  .fini_array start/end:    __fini_array_start__/__fini_array_end__     = %#X/%#X"), (uint32_t)&__fini_array_start, (uint32_t)&__fini_array_end);
    log_info(F("  Program end markers:  __end__       = %#X"), (uint32_t)&__end__);
    log_info(F("  Heap limits:          __HeapLimit   = %#X"), (uint32_t)&__HeapLimit);
    log_info(F("  Stack limits CORE0:         __StackLimit  = %#X; __StackTop = %#X; __StackBottom = %#X"), (uint32_t)&__StackLimit, (uint32_t)&__StackTop, (uint32_t)&__StackBottom);
    log_info(F("  Stack limits CORE1:         __StackLimit  = %#X; __StackTop = %#X; __StackBottom = %#X"), (uint32_t)&__StackLimit, (uint32_t)&__StackOneTop, (uint32_t)&__StackOneBottom);
    log_info(F("  Scratch RAM start:    __scratch_x_start__ = %#X; __scratch_y_start__ = %#X"), __scratch_x_source__, __scratch_y_source__);
#endif
}

/**
 * Logs the current system state, including system status and formatted uptime.
 */
void logSystemState() {
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;
    char buf[20];
    const unsigned long uptime = millis();
    snprintf(buf, 16, "%3luD %2luH %2lum", uptime/86400000l, (uptime/3600000l%24), (uptime/60000%60));
    log_info(F("System state: %#hX; uptime %s"), sysInfo->getSysStatus(), buf);
#endif
}

// SysInfo
SysInfo::SysInfo() : boardName(BOARD_NAME), deviceName(DEVICE_NAME), buildVersion(BUILD_VERSION), buildTime(BUILD_TIME), scmBranch(GIT_BRANCH),
                     ipAddress({IP_ADDR}), ipGateway({IP_GW}) {
    boardId.reserve(BUF_ID_SIZE);       // flash unique ID in hex, \0 terminator
    secElemId.reserve(BUF_ID_SIZE);     // 18 from ECCX08Class::serialNumber() implementation
    macAddress.reserve(BUF_ID_SIZE);    // 6 (WL_MAC_ADDR_LENGTH) groups of 2 hex digits and ':' separator, includes \0 terminator
    strIpAddress.reserve(BUF_ID_SIZE);     // 4 groups of 3 digits, 3 '.' separators, \0 terminator
    wifiFwVersion.reserve(BUF_ID_SIZE); // typical semantic version e.g., v1.5.0, 3 groups of 2 digits, '.' separator, \0 terminator
    ssid.reserve(BUF_ID_SIZE);          // initial space, most networks are short names
    cpuFrequency = 0;
    cpuVersion = 0;
    cpuModel.reserve(BUF_ID_SIZE);
    psramSize = 0;
    status = SysStatus::None;
    cleanBoot = true;
}

/**
 * Capture Flash UID as board unique identifier in HEX string format
 */
void SysInfo::fillBoardId() {
    boardId = rp2040.getChipID();
    cpuFrequency = RP2040::f_cpu();
#if defined(PICO_RP2350)
    cpuModel = "RP2350";
    cpuVersion = rp2350_chip_version();
    psramSize = rp2040.getPSRAMSize();
#elif defined(ARDUINO_ARCH_RP2040)
    cpuModel = "RP2040";
    cpuVersion = rp2040_rom_version();
#endif

}

/**
 * <p>Per AT25SF128A Flash specifications, command ox9F returns 0x1F8901, where last byte 0x01 should represent density (size)
 * Trial/error shows the command returns 0xFF1F89011F</p>
 * <p>We won't use the flash chip SPI commands - very chip-specific - but instead leverage the constant PICO_FLASH_SIZE_BYTES already tailored to the board we use, Nano RP2040</p>
 * @return Board flash size in bytes - currently fixed at PICO_FLASH_SIZE_BYTES
 */
uint SysInfo::get_flash_capacity() const {
//    uint8_t txbuf[STORAGE_CMD_TOTAL_BYTES] = {0x9f};
//    uint8_t rxbuf[STORAGE_CMD_TOTAL_BYTES] = {0};
//    flash_do_cmd(txbuf, rxbuf, STORAGE_CMD_TOTAL_BYTES);

//    return 1 << rxbuf[3];
    return PICO_FLASH_SIZE_BYTES;
}

SysStatus SysInfo::setSysStatus(const SysStatus bitMask) {
    CoreMutex coreMutex(&mutex);
    const SysStatus oldStatus = status;
    status |= bitMask;
    if (status != oldStatus) {
        sysInfoDirty = true;
    }
    return status;
}

SysStatus SysInfo::resetSysStatus(const SysStatus bitMask) {
    CoreMutex coreMutex(&mutex);
    const SysStatus oldStatus = status;
    status &= (~bitMask);
    if (status != oldStatus) {
        sysInfoDirty = true;
    }
    return status;
}

bool SysInfo::isSysStatus(const SysStatus bitMask) const {
    CoreMutex coreMutex(&mutex);
    return (status & bitMask) == bitMask;
}

SysStatus SysInfo::getSysStatus() const {
    CoreMutex coreMutex(&mutex);
    return status;
}

void SysInfo::addWatchdogReboot(const time_t t) {
    CoreMutex coreMutex(&mutex);
    wdReboots.push(t);
    sysInfoDirty = true;
}

size_t SysInfo::watchdogRebootsCount() const {
    CoreMutex coreMutex(&mutex);
    return wdReboots.size();
}

bool SysInfo::hasWatchdogReboots() const {
    CoreMutex coreMutex(&mutex);
    return !wdReboots.empty();
}

time_t SysInfo::lastWatchdogReboot() const {
    CoreMutex coreMutex(&mutex);
    return wdReboots.empty() ? 0 : wdReboots.back();
}

std::vector<time_t> SysInfo::watchdogRebootsSnapshot() const {
    CoreMutex coreMutex(&mutex);
    std::vector<time_t> snapshot;
    snapshot.reserve(wdReboots.size());
    for (const auto &t : wdReboots)
        snapshot.push_back(t);
    return snapshot;
}

void SysInfo::transformWatchdogReboots(const std::function<time_t(time_t)>& transform) {
    CoreMutex coreMutex(&mutex);
    for (auto &t : wdReboots)
        t = transform(t);
}

/**
 * Extracts identification information from a connected Wi-Fi.
 * NOTE: For reasons unknown yet, reading the network information from the Wi-Fi subsystem (through SPI connection) freezes the whole system
 * if the Analog/Digital Converter API calls are present somewhere else in the code. These Wi-Fi network information calls have SPI responses with 3 parameters,
 * unsure if this is a factor in failure - all other SPI calls with Wi-Fi module seem to be working fine, and they have fewer parameters.
 * For this reason, the workaround is to record the IP address and Gateway Address from the configuration provided (we're using static IP assignment) rather
 * than retrieving from the Wi-Fi module.
 * @param wifi the Wi-Fi (global) object
 */
void SysInfo::setWiFiInfo(::WiFiClass &wifi) {
    const String oldSsid = ssid;
    const String oldIp = strIpAddress;
    const String oldGw = strGatewayIpAddress;

    ssid = wifi.SSID();
    wifiFwVersion = ::WiFiClass::firmwareVersion();
    strIpAddress = wifi.localIP().toString();
    strGatewayIpAddress = wifi.gatewayIP().toString();
    // strIpAddress = ipAddress.toString();
    // strGatewayIpAddress = ipGateway.toString();

    if (ssid != oldSsid || strIpAddress != oldIp || strGatewayIpAddress != oldGw) {
        sysInfoDirty = true;
    }

    const IPAddress dns1 = wifi.dnsIP(0);
    const IPAddress dns2 = wifi.dnsIP(1);
    log_info(F("WiFi DNS servers: %s, %s"), dns1.toString().c_str(), dns2.toString().c_str());

    //MAC address - Formats the MAC address into the character buffer provided; space for 20 chars is needed (includes nul terminator)
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    wifi.macAddress(mac);
    char buf[BUF_ID_SIZE];
    int x = 0;
    for (const auto &b : mac)
        x += snprintf(buf+x, 4, "%02X:", b);
    //the last character - at index x-1 is a ':', make it null to trim the last colon character
    buf[x-1] = 0;
    macAddress = buf;
    sysInfoDirty = true;
}

void SysInfo::setSecureElementId(const String &secId) {
    if (secElemId != secId) {
        secElemId = secId;
        sysInfoDirty = true;
    }
}

void SysInfo::sysConfig(JsonDocument &doc) {
    doc["arduinoPicoVersion"] = ARDUINO_PICO_VERSION_STR;
    doc["freeRTOSVersion"] = tskKERNEL_VERSION_NUMBER;
    doc[csBoardName] = sysInfo->boardName;
    doc[csDeviceName] = sysInfo->deviceName;
    doc[csBoardId] = sysInfo->boardId;
    doc["psramSize"] = sysInfo->psramSize;
    doc["cpuModel"] = sysInfo->cpuModel;
    doc["cpuVersion"] = sysInfo->cpuVersion;
    doc["cpuFrequency"] = sysInfo->cpuFrequency;
    doc["cpuCore"] = RP2040::cpuid();
    doc[csSecElemId] = sysInfo->secElemId;
    doc[csBuildVersion] = sysInfo->buildVersion;
    doc[csScmBranch] = sysInfo->scmBranch;
    doc[csBuildTime] = sysInfo->buildTime;
    doc[csWifiFwVersion] = sysInfo->wifiFwVersion;
    doc["wifiLatestVersion"] = WIFI_FIRMWARE_LATEST_VERSION;
    doc[csMacAddress] = sysInfo->macAddress;
    doc[csIpAddress] = sysInfo->strIpAddress;
    doc[csGatewayAddress] = sysInfo->strGatewayIpAddress;
    doc[csStatus] = static_cast<uint16_t>(sysInfo->status);
    doc[csHeapSize] = sysInfo->heapSize;
    doc[csFreeHeap] = sysInfo->freeHeap;
    doc[csStackSize] = sysInfo->stackSize;
    doc[csFreeStack] = sysInfo->freeStack;
    const auto reboots = doc[csWdReboots].to<JsonArray>();
    for (const auto &t : sysInfo->watchdogRebootsSnapshot())
        (void)reboots.add(t);
}

/**
 * Generate Heap memory utilization statistics in JSON form
 * @param doc JSON object to populate
 */
void SysInfo::heapStats(JsonObject &doc) {
    // Simple heap stats
    HeapStats_t heapStats;
    vPortGetHeapStats(&heapStats);

    doc["stackPointer"] = rp2040.getStackPointer();
    doc["freeStack"] = sysInfo->freeStack = rp2040.getFreeStack();
    doc["totalHeap"] = sysInfo->heapSize = configTOTAL_HEAP_SIZE;
    doc["freeHeap"] = sysInfo->freeHeap = heapStats.xAvailableHeapSpaceInBytes;
    doc["usedHeap"] = (sysInfo->heapSize - sysInfo->freeHeap);
    doc["minHeap"] = heapStats.xMinimumEverFreeBytesRemaining;
    doc["maxHeapBlock"] = heapStats.xSizeOfLargestFreeBlockInBytes;
    doc["minHeapBlock"] = heapStats.xSizeOfSmallestFreeBlockInBytes;
    doc["freeBlocks"] = heapStats.xNumberOfFreeBlocks;
    doc["psramSize"] = sysInfo->psramSize;
#ifdef PICO_RP2350
    doc["psramHeapTotal"] = rp2040.getTotalPSRAMHeap();
    doc["psramHeapFree"] = rp2040.getFreePSRAMHeap();
    doc["psramHeapUsed"] = rp2040.getUsedPSRAMHeap();
#endif

#if LOGGING_ENABLED == 1
    doc["logMinBufferSpace"] = Log.getMinBufferSpace();
#endif
    //doc["watchdogRemaining"] = watchdog_get_time_remaining_ms();
}

/**
 * Generate task runtime statistics in JSON form
 * @param doc JSON array to populate
 */
void SysInfo::taskStats(JsonObject &doc) {
    TaskRuntimeSnapshot current;
    TaskRuntimeSnapshot previous;
    if (!loadTaskRuntimeHistory(current, previous))
        return;

    doc["count"] = current.tasks.size();
    doc["capturedAtMs"] = current.capturedAtMs;
    doc["windowSec"] = snapshotWindowSec(current, previous);
    doc["sysTotalRunTime"] = current.sysTotalRunTimeRaw;
    doc["tasksTotalRunTime"] = current.totalRunTime;
    doc["totalCPULoadPct"] = cpuLoadPct(current, previous);

    const uint64_t totalDelta = runtimeDelta(current.totalRunTime, previous.totalRunTime);
    const auto jsArray = doc["items"].to<JsonArray>();
    for (const auto &taskStatus : current.tasks) {
        const TaskStatus_t *prevTaskStatus = findTaskStatus(previous.tasks, taskStatus.xTaskNumber);
        JsonObject task = jsArray.add<JsonObject>();
        const uint64_t taskDeltaTime = prevTaskStatus != nullptr
            ? runtimeDelta(taskStatus.ulRunTimeCounter, prevTaskStatus->ulRunTimeCounter)
            : taskStatus.ulRunTimeCounter;
        const uint coreAffinity = taskStatus.uxCoreAffinityMask >= CORE_ALL ? CORE_ALL : taskStatus.uxCoreAffinityMask;

        task["name"] = taskStatus.pcTaskName;
        task["state"] = taskStatusToString(taskStatus.eCurrentState);
        task["curPriority"] = taskStatus.uxCurrentPriority;
        task["basePriority"] = taskStatus.uxBasePriority;
        task["stackHighWaterMark"] = taskStatus.usStackHighWaterMark;
        task["taskNumber"] = taskStatus.xTaskNumber;
        task["coreAffinity"] = coreAffinity;
        task["runTime"] = taskDeltaTime;
        task["runTimeLife"] = taskStatus.ulRunTimeCounter;
        task["runTimePct"] = runtimePct(taskDeltaTime, totalDelta);
    }
}

/**
 * Read the saved sys info file - no op, if there is no file
 * Note the time this is performed, the WiFi is not ready, nor other fields even populated yet
 */
void readSysInfo() {
    const auto json = new String();
    json->reserve(512);  // approximation
    if (const size_t sysSize = SyncFsImpl.readFile(sysFileName, json); sysSize > 0) {
        log_info(F("System information [%s]:\n%s"), sysFileName, json->c_str());
        JsonDocument doc;
        if (const DeserializationError error = deserializeJson(doc, *json)) {
            log_error(F("Error reading the system information JSON file %s [%zu bytes]: %s - system information state NOT restored. Content read:\n%s"), sysFileName, sysSize, error.c_str(), json->c_str());
            delete json;
            doc.clear();
            return;
        }
        //const fields
        const String bldVersion = doc[csBuildVersion];
        const String brdName = doc[csBoardName];
        const String devName = doc[csDeviceName] | DEVICE_NAME;
        const String bldTime = doc[csBuildTime];
        const auto gitBranch = doc[csScmBranch].as<String>();
        if (bldVersion.equals(sysInfo->buildVersion) && doc[csWdReboots].is<JsonArray>()) {
            const auto wdReboots = doc[csWdReboots].as<JsonArray>();
            for (JsonVariant i: wdReboots)
                sysInfo->addWatchdogReboot(i.as<time_t>());
        } else
            log_warn(F("Build version change detected - previous watchdog reboot timestamps %s have been discarded"), doc[csWdReboots].as<String>().c_str());
        sysInfo->boardId = doc[csBoardId].as<String>();
        sysInfo->secElemId = doc[csSecElemId].as<String>();
        sysInfo->macAddress = doc[csMacAddress].as<String>();
        sysInfo->wifiFwVersion = doc[csWifiFwVersion].as<String>();
        sysInfo->strIpAddress = doc[csIpAddress].as<String>();
        sysInfo->strGatewayIpAddress = doc[csGatewayAddress].as<String>();
        sysInfo->heapSize = doc[csHeapSize];
        sysInfo->freeHeap = doc[csFreeHeap];
        sysInfo->stackSize = doc[csStackSize];
        sysInfo->freeStack = doc[csFreeStack];
        //do not override the current status (in progress of populating) with the last run status
        const auto lastStatus = doc[csStatus].as<uint16_t>();
        log_info(F("System Information restored from %s [%d bytes]: boardName=%s, deviceName=%s, buildVersion=%s, buildTime=%s, scmBranch=%s, boardId=%s, secElemId=%s, macAddress=%s, status=%#hhX (last %#hhX), IP=%s, Gateway=%s"),
                   sysFileName, sysSize, brdName.c_str(), devName.c_str(), bldVersion.c_str(), bldTime.c_str(), gitBranch.c_str(), sysInfo->boardId.c_str(), sysInfo->secElemId.c_str(), sysInfo->macAddress.c_str(), sysInfo->status,
                   lastStatus, sysInfo->strIpAddress.c_str(), sysInfo->strGatewayIpAddress.c_str());
        doc.clear();
    } else
        log_info(F("System information file %s not found - system information will be re-built"), sysFileName);
    delete json;
}

/**
 * Saves the sys info to the file
 */
void saveSysInfo() {
    const uint32_t nowMs = millis();
    if (sysInfoSaveInProgress.load(std::memory_order_acquire)) {
        return;
    }

    if (!sysInfoDirty && sysInfoLastSaveMs != 0 && (nowMs - sysInfoLastSaveMs) < sysInfoSaveIntervalMs) {
        return;
    }

    bool expected = false;
    if (!sysInfoSaveInProgress.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    JsonDocument doc;
    SysInfo::sysConfig(doc);
    auto str = new String();    // larger temporary string, put it on the heap
    str->reserve(measureJson(doc));
    serializeJson(doc, *str);

    if (SyncFsImpl.writeFileAsync(sysFileName, str)) {
        sysInfoDirty = false;
        sysInfoLastSaveMs = nowMs;
    } else {
        log_error(F("Failed to enqueue async system information file write %s"), sysFileName);
        delete str;
    }

    doc.clear();

    // NOTE: sysconfig merge is intentionally skipped now to avoid extra blocking flash write activity
    // at each sysinfo tick, because this has correlated with watchdog-triggering delay spikes.

    sysInfoSaveInProgress.store(false, std::memory_order_release);
}

/**
 * Set-up the on-board status LED
 */
void SysInfo::setupStateLED() {
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    updateBoardLED(CRGB::Black);    //black, turned off
}
/**
 * Controls the on-board status LED
 * @param colorCode
 */
void SysInfo::updateBoardLED(const uint32_t colorCode) {
    updateBoardLED(CRGB(colorCode));
}

/**
 * Controls the onboard LED using individual values for R, G, B
 * On RPI based boards (Pico 2W, Plasma, etc), the LED(s) are simple GPIO-controlled LED.
 * On Plasma 2350 W, the RGB LED is on GPIO 16, 17, 18
 * On RPi Pico 2 W, the status LED is on GPIO 15
 * @param rgb RGB value
 */
void SysInfo::updateBoardLED(const CRGB rgb) {
    analogWrite(PIN_LED_R, 255 - rgb.red);
    analogWrite(PIN_LED_G, 255 - rgb.green);
    analogWrite(PIN_LED_B, 255 - rgb.blue);
}

/**
 * Adjusts the LED state (color, illumination style) in response to the overall system's state
 */
void SysInfo::updateStatusLED() const {
    const bool isOk = isSysStatus(SysStatus::Wifi | SysStatus::Ntp | SysStatus::Filesystem | SysStatus::Diag);
    const CRGB colorCode = isOk ? CLR_ALL_OK : !isSysStatus(SysStatus::Setup0 | SysStatus::Setup1) ? CLR_SETUP_IN_PROGRESS : CLR_SETUP_ERROR;
    updateBoardLED(colorCode);
}

/**
 * Flash status LED for as long as both cores are in setup mode
 */
void state_led_begin() {
    while (!sysInfo->isSysStatus(SysStatus::Setup0 | SysStatus::Setup1)) {
        SysInfo::updateBoardLED(CRGB::Black);
        taskDelay(640);
        SysInfo::updateBoardLED(CLR_SETUP_IN_PROGRESS);
        taskDelay(640);
    }
}

/**
 * Update the color of the status LED consistent with the system's state
 */
void state_led_update() {
    sysInfo->updateStatusLED();
}

void SysInfo::begin() {
}
