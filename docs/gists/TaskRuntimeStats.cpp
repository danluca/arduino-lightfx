#include "TaskRuntimeStats.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

#ifndef log_info
#error "Define log_info(fmt, ...) before compiling TaskRuntimeStats.cpp"
#endif

#ifndef log_write
#error "Define log_write(level, text) before compiling TaskRuntimeStats.cpp"
#endif

#ifndef LOG_LEVEL_INFO
#error "Define LOG_LEVEL_INFO to the log level value expected by log_write(level, text)"
#endif

#ifndef log_is_enabled
#define log_is_enabled() true
#endif

namespace {

constexpr UBaseType_t kTaskSnapshotSlack = 2;
constexpr unsigned long kTaskSnapshotIntervalMs = 5000ul;
constexpr unsigned long kTaskSnapshotMaxAgeMs = kTaskSnapshotIntervalMs * 2;
constexpr unsigned long kTaskSnapshotForceMinIntervalMs = 10000ul;

bool isSnapshotFresh(const TaskRuntimeSnapshot &snapshot, const unsigned long nowMs) {
    return snapshot.valid && (nowMs - snapshot.capturedAtMs) <= kTaskSnapshotMaxAgeMs;
}

bool compareTasksByNumber(const TaskStatus_t &lhs, const TaskStatus_t &rhs) {
    return lhs.xTaskNumber < rhs.xTaskNumber;
}

} // namespace

TaskRuntimeMonitor::TaskRuntimeMonitor() {
    mutex_init(&history_.stateMutex);
    mutex_init(&history_.captureMutex);
}

TaskRuntimeMonitor &TaskRuntimeMonitor::instance() {
    static TaskRuntimeMonitor monitor;
    return monitor;
}

bool isIdleTaskName(const char *taskName) {
    return taskName != nullptr && std::strstr(taskName, "IDLE") != nullptr;
}

const TaskStatus_t *findTaskStatus(const std::vector<TaskStatus_t> &tasks, const UBaseType_t taskNumber) {
    for (const auto &task : tasks) {
        if (task.xTaskNumber == taskNumber) {
            return &task;
        }
    }
    return nullptr;
}

uint64_t runtimeDelta(const uint64_t currentValue, const uint64_t previousValue) {
    return currentValue >= previousValue ? currentValue - previousValue : 0;
}

float runtimePct(const uint64_t runtimeValue, const uint64_t totalRuntime) {
    return totalRuntime > 0
        ? static_cast<float>(runtimeValue) * 100.0f / static_cast<float>(totalRuntime)
        : 0.0f;
}

float cpuLoadPct(const TaskRuntimeSnapshot &current, const TaskRuntimeSnapshot &previous) {
    const uint64_t totalDelta = runtimeDelta(current.taskTotalRunTime, previous.taskTotalRunTime);
    const uint64_t idleDelta = runtimeDelta(current.idleRunTime, previous.idleRunTime);
    return runtimePct(totalDelta > idleDelta ? totalDelta - idleDelta : 0, totalDelta);
}

float snapshotWindowSec(const TaskRuntimeSnapshot &current, const TaskRuntimeSnapshot &previous) {
    return previous.valid
        ? static_cast<float>(current.capturedAtMs - previous.capturedAtMs) / 1000.0f
        : 0.0f;
}

const char *taskStateName(const eTaskState state) {
    switch (state) {
        case eReady: return "RDY";
        case eBlocked: return "BLK";
        case eSuspended: return "SPN";
        case eDeleted: return "DEL";
        case eRunning: return "RUN";
        case eInvalid: return "INV";
        default: return "N/A";
    }
}

bool TaskRuntimeMonitor::populate(TaskRuntimeSnapshot &snapshot) {
    snapshot = TaskRuntimeSnapshot{};

    for (uint8_t attempt = 0; attempt < 2; ++attempt) {
        const UBaseType_t taskCount = uxTaskGetNumberOfTasks();
        if (taskCount == 0) {
            return false;
        }

        const UBaseType_t capacity = taskCount + kTaskSnapshotSlack;
        snapshot.tasks.assign(capacity, TaskStatus_t{});
        snapshot.systemTotalRunTime = 0;

        const UBaseType_t actualCount = uxTaskGetSystemState(
            snapshot.tasks.data(),
            capacity,
            &snapshot.systemTotalRunTime);

        if (actualCount == 0) {
            snapshot.tasks.clear();
            return false;
        }

        if (actualCount >= capacity && uxTaskGetNumberOfTasks() > capacity && attempt == 0) {
            continue;
        }

        snapshot.tasks.resize(actualCount);
        std::sort(snapshot.tasks.begin(), snapshot.tasks.end(), compareTasksByNumber);

        for (const auto &task : snapshot.tasks) {
            snapshot.taskTotalRunTime += task.ulRunTimeCounter;
            if (isIdleTaskName(task.pcTaskName)) {
                snapshot.idleRunTime += task.ulRunTimeCounter;
            }
        }
        //NOTE: This requires HEAP 4 scheme (Heap_4.c) to be enabled (note the default is Heap_3a.c) - comment out if using a different heap scheme
        vPortGetHeapStats(&snapshot.heapStats);
        snapshot.capturedAtMs = millis();
        snapshot.valid = true;
        return true;
    }

    snapshot.tasks.clear();
    return false;
}

bool TaskRuntimeMonitor::copyHistory(TaskRuntimeSnapshot &current, TaskRuntimeSnapshot &previous) const {
    CoreMutex lock(&history_.stateMutex);
    if (!history_.current.valid) {
        return false;
    }

    current = history_.current;
    previous = history_.previous;
    return true;
}

bool TaskRuntimeMonitor::capture(const bool force) {
    const unsigned long nowMs = millis();

    if (force) {
        CoreMutex stateLock(&history_.stateMutex);
        if (isSnapshotFresh(history_.current, nowMs)) {
            return true;
        }
        if (history_.current.valid &&
            history_.lastForcedCaptureMs != 0 &&
            (nowMs - history_.lastForcedCaptureMs) < kTaskSnapshotForceMinIntervalMs) {
            return true;
        }
        history_.lastForcedCaptureMs = nowMs;
    }

    CoreMutex captureLock(&history_.captureMutex);

    if (force) {
        TaskRuntimeSnapshot current;
        TaskRuntimeSnapshot previous;
        if (copyHistory(current, previous) && isSnapshotFresh(current, millis())) {
            return true;
        }
    }

    TaskRuntimeSnapshot snapshot;
    if (!populate(snapshot)) {
        return false;
    }

    CoreMutex stateLock(&history_.stateMutex);
    history_.previous = history_.current;
    history_.current = std::move(snapshot);
    if (force) {
        history_.lastForcedCaptureMs = history_.current.capturedAtMs;
    }
    return true;
}

bool TaskRuntimeMonitor::load(TaskRuntimeSnapshot &current, TaskRuntimeSnapshot &previous) {
    if (copyHistory(current, previous) && isSnapshotFresh(current, millis())) {
        return true;
    }
    if (!capture(true)) {
        return copyHistory(current, previous);
    }
    return copyHistory(current, previous);
}

bool captureTaskRuntimeSnapshot(const bool force) {
    return TaskRuntimeMonitor::instance().capture(force);
}

void logTaskRuntimeStats() {
    // If logging is disabled, this function is effectively a no-op, so we can skip the capture and formatting work
    // If desired to compile out when logging is disabled, wrap the entire function body in #if LOGGING_ENABLED == 1 / #endif and remove the log_is_enabled() check
    if (!log_is_enabled()) {
        return;
    }

    if (!TaskRuntimeMonitor::instance().capture(false)) {
        return;
    }

    TaskRuntimeSnapshot current;
    TaskRuntimeSnapshot previous;
    if (!TaskRuntimeMonitor::instance().load(current, previous)) {
        return;
    }

    const uint64_t totalDelta = runtimeDelta(current.taskTotalRunTime, previous.taskTotalRunTime);

    log_info("TASK STATS [sysRunTime=%llu delta=%llu capturedAt=%lu ms taskCycles=%llu/%llu]",
        static_cast<unsigned long long>(current.systemTotalRunTime),
        static_cast<unsigned long long>(totalDelta),
        current.capturedAtMs,
        static_cast<unsigned long long>(current.taskTotalRunTime),
        static_cast<unsigned long long>(previous.taskTotalRunTime));
    log_write(LOG_LEVEL_INFO, "Name      \tState    \tPr \tStk     Num \tCore  RunTime       RunPct\n");

    for (const auto &task : current.tasks) {
        const TaskStatus_t *previousTask = findTaskStatus(previous.tasks, task.xTaskNumber);
        const uint64_t taskDelta = previousTask != nullptr
            ? runtimeDelta(task.ulRunTimeCounter, previousTask->ulRunTimeCounter)
            : task.ulRunTimeCounter;
        const float taskPct = runtimePct(taskDelta, totalDelta);
        const char priorityChange = task.uxCurrentPriority > task.uxBasePriority
            ? '+'
            : task.uxCurrentPriority < task.uxBasePriority ? '-' : ' ';
        const UBaseType_t coreAffinity = task.uxCoreAffinityMask;

        char line[112];
        std::snprintf(line, sizeof(line), "%-10s\t%-8s\t%u%c\t%-6u  %-4u\t0x%02x  %-12llu  %.2f %%\n",
            task.pcTaskName,
            taskStateName(task.eCurrentState),
            static_cast<unsigned>(task.uxCurrentPriority),
            priorityChange,
            static_cast<unsigned>(task.usStackHighWaterMark),
            static_cast<unsigned>(task.xTaskNumber),
            static_cast<unsigned>(coreAffinity),
            static_cast<unsigned long long>(taskDelta),
            taskPct);
        log_write(LOG_LEVEL_INFO, line);
    }

    char line[128];
    std::snprintf(line, sizeof(line),
        "\nTotal CPU Load: %.2f %% / %.2f s\nHeap: free=%zu low=%zu blocks=%zu largest=%zu\n",
        cpuLoadPct(current, previous),
        snapshotWindowSec(current, previous),
        current.heapStats.xAvailableHeapSpaceInBytes,
        current.heapStats.xMinimumEverFreeBytesRemaining,
        current.heapStats.xNumberOfFreeBlocks,
        current.heapStats.xSizeOfLargestFreeBlockInBytes);
    log_write(LOG_LEVEL_INFO, line);
}

void logTaskRuntimeSummary() {
    // If logging is disabled, this function is effectively a no-op, so we can skip the capture and formatting work
    // If desired to compile out when logging is disabled, wrap the entire function body in #if LOGGING_ENABLED == 1 / #endif and remove the log_is_enabled() check
    if (!log_is_enabled()) {
        return;
    }

    if (!TaskRuntimeMonitor::instance().capture(false)) {
        return;
    }

    TaskRuntimeSnapshot current;
    TaskRuntimeSnapshot previous;
    if (!TaskRuntimeMonitor::instance().load(current, previous)) {
        return;
    }

    log_info("TASK SUMMARY: tasks=%u cpuLoad=%.2f%% window=%.2fs heapFree=%zu heapLow=%zu freeBlocks=%zu largestFree=%zu taskCycles=%llu/%llu",
        static_cast<unsigned>(current.tasks.size()),
        cpuLoadPct(current, previous),
        snapshotWindowSec(current, previous),
        current.heapStats.xAvailableHeapSpaceInBytes,
        current.heapStats.xMinimumEverFreeBytesRemaining,
        current.heapStats.xNumberOfFreeBlocks,
        current.heapStats.xSizeOfLargestFreeBlockInBytes,
        static_cast<unsigned long long>(current.taskTotalRunTime),
        static_cast<unsigned long long>(previous.taskTotalRunTime));
}
