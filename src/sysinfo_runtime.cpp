// Copyright (c) by Dan Luca. All rights reserved.
//
#include <Arduino.h>
#include <ArduinoJson.h>
#include <malloc.h>

#include <SchedulerExt.h>

#include "config.h"
#include "constants.hpp"
#include "log.h"
#include "sysinfo_internal.h"
#include "timeutil.h"

#if LOGGING_ENABLED == 1
#include <stringutils.h>
#endif

namespace {

constexpr UBaseType_t kTaskSnapshotSlack = 2;
constexpr unsigned long kTaskSnapshotIntervalMs = 5000ul;
constexpr unsigned long kTaskSnapshotMaxAgeMs = kTaskSnapshotIntervalMs * 2;
constexpr unsigned long kTaskSnapshotForceMinIntervalMs = 10000ul;

#if LOGGING_ENABLED == 1
constexpr auto heapStackInfoFmt = "APP HEAP/STACK INFO\n  Stack     :: ptr=%#X;\n  Heap      :: size=%zu used=%zu free=%zu lowest=%zu block max/min/free=%zu/%zu/%zu\n";
constexpr auto heapPSRAMInfoFmt = "  PSRAM Heap:: PSRAM=%zu size=%d (free=%d used=%d)\n";
constexpr auto newlibHeapInfoFmt = "NEWLIB/OS HEAP INFO\n  Heap      :: size=%zu used=%zu free=%zu usage=%.1f%% \n";
constexpr auto fmtTaskInfo = "%-10s\t%s\t%u%c\t%-6u  %-4u\t0x%02x  %-12llu  %.2f %%\n";
constexpr auto fmtTotalCPULoad = "\nTotal CPU Load:    %.2f %% / %.2f s\n";
#endif

/**
 * Checks if task runtime snapshot is fresh based on age - threshold configured by kTaskSnapshotMaxAgeMs
 * @param snapshot task runtime snapshot
 * @param nowMs current time in milliseconds
 * @return true if the snapshot is fresh, false otherwise
 */
bool isSnapshotFresh(const TaskRuntimeSnapshot &snapshot, const unsigned long nowMs) {
    return snapshot.valid && (nowMs - snapshot.capturedAtMs) <= kTaskSnapshotMaxAgeMs;
}

/**
 * Generic comparator of tasks by number - works with TaskStatus_t pointers
 * @param a TaskStatus_t pointer A to compare
 * @param b TaskStatus_t pointer B to compare
 * @return result of comparing task A number to task B number
 */
int compareTasksByNumber(const void *a, const void *b) {
    const auto *taskA = static_cast<const TaskStatus_t *>(a);
    const auto *taskB = static_cast<const TaskStatus_t *>(b);
    return static_cast<int>(taskA->xTaskNumber) - static_cast<int>(taskB->xTaskNumber);
}

TaskRuntimeMonitor g_taskRuntimeMonitor{};

} // namespace

/**
 * Checks if task name is an idle task name - i.e. named IDLE0 or IDLE1
 * @param taskName task name to check
 * @return true if task name is an idle task name, false otherwise
 */
bool isIdleTaskName(const char *taskName) {
    if (taskName == nullptr)
        return false;
    return strstr(taskName, "IDLE") != nullptr;
}

/**
 * Finds task with matching number in task status collection
 * @param taskStatusArray task status collection
 * @param taskNumber the task number to find
 * @return task with matching number or nullptr
 */
const TaskStatus_t *findTaskStatus(const std::vector<TaskStatus_t> &taskStatusArray, const UBaseType_t taskNumber) {
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
uint64_t runtimeDelta(const uint64_t currentValue, const uint64_t previousValue) {
    return currentValue >= previousValue ? currentValue - previousValue : 0;
}

/**
 * Calculates the percentage of runtime value relative to total runtime
 * @param runtimeValue runtime value
 * @param totalRuntime total runtime
 * @return runtime percentage
 */
float runtimePct(const uint64_t runtimeValue, const uint64_t totalRuntime) {
    return totalRuntime > 0 ? static_cast<float>(runtimeValue) * 100.0f / static_cast<float>(totalRuntime) : 0.0f;
}

/**
 * Calculates the CPU load percentage based on task runtime snapshots
 * @param current current task snapshot
 * @param previous previous task snapshot
 * @return CPU load percentage
 */
float cpuLoadPct(const TaskRuntimeSnapshot &current, const TaskRuntimeSnapshot &previous) {
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
float snapshotWindowSec(const TaskRuntimeSnapshot &current, const TaskRuntimeSnapshot &previous) {
    return previous.valid
        ? static_cast<float>(current.capturedAtMs - previous.capturedAtMs) / 1000.0f
        : 0.0f;
}

TaskRuntimeMonitor &TaskRuntimeMonitor::instance() {
    return g_taskRuntimeMonitor;
}

/**
 * Populates task runtime snapshot by capturing task status and system total runtime
 * @param snapshot task runtime snapshot to populate
 * @return true if snapshot was successfully populated, false otherwise
 */
bool TaskRuntimeMonitor::populate(TaskRuntimeSnapshot &snapshot) {
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
        const struct mallinfo mf = mallinfo();
        snapshot.mallocStats.size = mf.arena;
        snapshot.mallocStats.used = mf.uordblks;
        snapshot.mallocStats.available = mf.fordblks;

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
bool TaskRuntimeMonitor::copyHistory(TaskRuntimeSnapshot &current, TaskRuntimeSnapshot &previous) const {
    CoreMutex lock(&history_.stateMutex);
    if (!history_.current.valid)
        return false;

    current = history_.current;
    previous = history_.previous;
    return true;
}

/**
 * Captures task runtime snapshot, updating current and previous snapshots
 * @param force whether to force capture regardless of freshness
 * @return true if capture was successful, false otherwise
 */
bool TaskRuntimeMonitor::capture(const bool force) {
    const unsigned long nowMs = millis();
    if (force) {
        CoreMutex stateLock(&history_.stateMutex);
        if (isSnapshotFresh(history_.current, nowMs))
            return true;
        if (history_.current.valid &&
            history_.lastForcedCaptureMs != 0 &&
            (nowMs - history_.lastForcedCaptureMs) < kTaskSnapshotForceMinIntervalMs)
            return history_.current.valid;
        history_.lastForcedCaptureMs = nowMs;
    }

    CoreMutex captureLock(&history_.captureMutex);
    if (force) {
        TaskRuntimeSnapshot current;
        TaskRuntimeSnapshot previous;
        if (copyHistory(current, previous) && isSnapshotFresh(current, millis()))
            return true;
    }

    TaskRuntimeSnapshot snapshot;
    if (!populate(snapshot))
        return false;

    CoreMutex stateLock(&history_.stateMutex);
    history_.previous = history_.current;
    history_.current = std::move(snapshot);
    if (force)
        history_.lastForcedCaptureMs = history_.current.capturedAtMs;
    return true;
}

/**
 * Loads task runtime history by copying current to previous snapshot and capturing if necessary
 * @param current current task runtime snapshot
 * @param previous previous task runtime snapshot
 * @return true if load was successful, false otherwise
 */
bool TaskRuntimeMonitor::load(TaskRuntimeSnapshot &current, TaskRuntimeSnapshot &previous) {
    if (copyHistory(current, previous) && isSnapshotFresh(current, millis()))
        return true;
    if (!capture(true))
        return copyHistory(current, previous);
    return copyHistory(current, previous);
}

bool captureTaskRuntimeSnapshot(const bool force) {
    return TaskRuntimeMonitor::instance().capture(force);
}

/**
 * Logs detailed information about FreeRTOS task statistics and heap usage.
 *
 * The task information includes details such as the name of each task, state, priority,
 * runtime percentages, stack usage, core affinity, and runtime counters across cores.
 *
 * Heap statistics report total stack size, free stack space, heap size, and usage.
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
    if (!TaskRuntimeMonitor::instance().load(current, previous))
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
    logHeapStats();
    struct mallinfo mf = mallinfo();
    log_info(newlibHeapInfoFmt, mf.arena, mf.uordblks, mf.fordblks, (float)mf.uordblks / mf.arena * 100.0f);
    log_info(F("Minimum log buffer free space %zu bytes"), Log.getMinFreeSpace());
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
 */
void logTaskSummary() {
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;

    TaskRuntimeSnapshot current;
    TaskRuntimeSnapshot previous;
    if (!TaskRuntimeMonitor::instance().load(current, previous))
        return;

    log_info(F("TASK SUMMARY: tasks=%u cpuLoad=%.2f %% window=%.2f s appHeapUsed=%.2f %% minAppHeap=%zu maxBlockFree=%zu newlibHeapUsed=%.2f %% newlibHeapFree=%zu task cycles cur/prev %llu / %llu"),
        static_cast<unsigned>(current.tasks.size()), cpuLoadPct(current, previous), snapshotWindowSec(current, previous),
        (configTOTAL_HEAP_SIZE - current.heapStats.xAvailableHeapSpaceInBytes)*100.0f/configTOTAL_HEAP_SIZE, current.heapStats.xMinimumEverFreeBytesRemaining,
        current.heapStats.xSizeOfLargestFreeBlockInBytes, current.mallocStats.used*100.0f/current.mallocStats.size,
        current.mallocStats.available, current.totalRunTime, previous.totalRunTime);
#endif
}

/**
 * Log Heap memory allocation stats
 */
void logHeapStats() {
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;

    String strHeapInfo;
    strHeapInfo.reserve(256);
    HeapStats_t heapStats;
    vPortGetHeapStats(&heapStats);
    StringUtils::append(strHeapInfo, heapStackInfoFmt, rp2040.getStackPointer(), configTOTAL_HEAP_SIZE, (configTOTAL_HEAP_SIZE - heapStats.xAvailableHeapSpaceInBytes), heapStats.xAvailableHeapSpaceInBytes,
        heapStats.xMinimumEverFreeBytesRemaining, heapStats.xSizeOfLargestFreeBlockInBytes, heapStats.xSizeOfSmallestFreeBlockInBytes, heapStats.xNumberOfFreeBlocks);
#ifdef PICO_RP2350
    StringUtils::append(strHeapInfo, heapPSRAMInfoFmt, rp2040.getPSRAMSize(), rp2040.getTotalPSRAMHeap(), rp2040.getFreePSRAMHeap(), rp2040.getUsedPSRAMHeap());
#endif
    log_info(F("%s"), strHeapInfo.c_str());
#endif
}

/**
 * Generate Heap memory utilization statistics in JSON form
 * @param doc JSON object to populate
 */
void SysInfo::heapStats(JsonObject &doc) {
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
    doc["logMinBufferSpace"] = Log.getMinFreeSpace();
#endif
}

/**
 * Generate task runtime statistics in JSON form
 * @param doc JSON array to populate
 */
void SysInfo::taskStats(JsonObject &doc) {
    TaskRuntimeSnapshot current;
    TaskRuntimeSnapshot previous;
    if (!TaskRuntimeMonitor::instance().load(current, previous))
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
