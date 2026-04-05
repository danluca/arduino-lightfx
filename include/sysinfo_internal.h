#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <task.h>
#include <vector>

#include "sysinfo.h"

/**
 * Represents a snapshot of task runtime statistics and system resource usage at a specific point in time.
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

class TaskRuntimeMonitor {
public:
    static TaskRuntimeMonitor &instance();

    bool capture(bool force);
    bool load(TaskRuntimeSnapshot &current, TaskRuntimeSnapshot &previous);

private:
    //Maintains a historical record of task runtime snapshots for diagnostics and analysis.
    struct TaskRuntimeHistory {
        mutable mutex_t stateMutex{};
        mutable mutex_t captureMutex{};
        TaskRuntimeSnapshot current{};
        TaskRuntimeSnapshot previous{};
        unsigned long lastForcedCaptureMs{0};
    };

    bool copyHistory(TaskRuntimeSnapshot &current, TaskRuntimeSnapshot &previous) const;

    static bool populate(TaskRuntimeSnapshot &snapshot);

    TaskRuntimeHistory history_{};
};

class SysInfoPersistence {
public:
    static SysInfoPersistence &instance();

    void markDirty();

    static void read();
    void save();

private:
    bool dirty_{true};
    uint32_t lastSaveMs_{0};
};

const TaskStatus_t *findTaskStatus(const std::vector<TaskStatus_t> &taskStatusArray, UBaseType_t taskNumber);
uint64_t runtimeDelta(uint64_t currentValue, uint64_t previousValue);
float runtimePct(uint64_t runtimeValue, uint64_t totalRuntime);
float cpuLoadPct(const TaskRuntimeSnapshot &current, const TaskRuntimeSnapshot &previous);
float snapshotWindowSec(const TaskRuntimeSnapshot &current, const TaskRuntimeSnapshot &previous);
bool isIdleTaskName(const char *taskName);
