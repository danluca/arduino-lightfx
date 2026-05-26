#pragma once

/*
 * Minimal FreeRTOS task runtime monitor/logger for Raspberry Pi Pico/Pico 2
 * using Earle Philhower's arduino-pico core.
 *
 * Expected project setup:
 *   - Build with Earle Philhower's arduino-pico core, which provides CoreMutex.
 *   - FreeRTOS task stats are enabled:
 *       configUSE_TRACE_FACILITY == 1
 *       configGENERATE_RUN_TIME_STATS == 1
 *       configUSE_STATS_FORMATTING_FUNCTIONS == 1 (only if you also use FreeRTOS formatting helpers)
 *   - The sketch/project provides logging macros before compiling TaskRuntimeStats.cpp:
 *       log_info(fmt, ...)
 *       log_write(level, text)
 *       LOG_LEVEL_INFO (or equivalent value expected by log_write for info level)
 *     Optional:
 *       log_is_enabled()   // defaults to true if omitted
 *
 * Example logging adapter:
 *   #define LOG_LEVEL_INFO 1
 *   #define log_is_enabled() true
 *   #define log_info(fmt, ...) Serial.printf("%lu I: ", millis()); Serial.printf((String(fmt) + "\n").c_str(), ##__VA_ARGS__)
 *   #define log_write(level, text) Serial.print(text)
 * 
 * Example output format for logTaskRuntimeStats:
 * 49912655 I: TASK STATS [sysRunTime=2668015123, delta=19274695, capturedAt=49912650 ms]
 *
 * Name      	St 	Pr 	Stk     Num 	Core  RunTime       RunPct
 * USB       	Blk	10 	176     1   	0x01  461036        2.39 %
 * CORE0     	Blk	5 	2524    2   	0x01  119041        0.62 %
 * IdleCore0 	Blk	11 	98      3   	0x01  0             0.00 %
 * IdleCore1 	Blk	11 	88      4   	0x02  0             0.00 %
 * IDLE0     	Run	0 	222     5   	0xff  7885192       40.91 %
 * IDLE1     	Rdy	0 	226     6   	0xff  7890526       40.94 %
 * Tmr Svc   	Blk	2 	972     7   	0xff  721           0.00 %
 * LWIP      	Blk	10 	482     8   	0x01  187373        0.97 %
 * CORE1     	Run	6 	2558    9   	0x02  3568          0.02 %
 * SRL       	Blk	4 	800     10  	0xff  3025          0.02 %
 * FS        	Blk	7 	1093    11  	0x01  0             0.00 %
 * ALM       	Blk	5 	1144    12  	0x01  0             0.00 %
 * Fx        	Blk	7 	1089    13  	0x02  2665997       13.83 %
 * EthPoll   	Blk	1 	186     14  	0xff  58216         0.30 %

 * Total CPU Load:    18.15 % / 9.64 s
 * Heap: free=171776 low=159008 blocks=10 largest=171464
 * 
 * Example output format for logTaskRuntimeSummary:
 * 49912678 I: TASK SUMMARY: tasks=14 cpuLoad=18.15 % window=9.64 s heapFree=171776 heapLow=159008 freeBlocks=10 largestFree=171464 taskCycles=99825287784/99806013089
 */

#include <Arduino.h>
#include <FreeRTOS.h>
#include <CoreMutex.h>
#include <pico/mutex.h>
#include <task.h>

#include <vector>

struct TaskRuntimeSnapshot {
    std::vector<TaskStatus_t> tasks{};
    configRUN_TIME_COUNTER_TYPE systemTotalRunTime{0};
    uint64_t taskTotalRunTime{0};
    uint64_t idleRunTime{0};
    HeapStats_t heapStats{};
    unsigned long capturedAtMs{0};
    bool valid{false};
};

class TaskRuntimeMonitor {
public:
    static TaskRuntimeMonitor &instance();

    TaskRuntimeMonitor();

    bool capture(bool force = false);
    bool load(TaskRuntimeSnapshot &current, TaskRuntimeSnapshot &previous);

private:
    struct History {
        mutex_t stateMutex{};
        mutex_t captureMutex{};
        TaskRuntimeSnapshot current{};
        TaskRuntimeSnapshot previous{};
        unsigned long lastForcedCaptureMs{0};
    };

    bool copyHistory(TaskRuntimeSnapshot &current, TaskRuntimeSnapshot &previous) const;
    static bool populate(TaskRuntimeSnapshot &snapshot);

    mutable History history_{};
};

bool captureTaskRuntimeSnapshot(bool force = false);

// Call these from a diagnostics/maintenance task at your chosen interval. Each
// call captures a fresh sample and compares it with the previous sample.
void logTaskRuntimeStats();
void logTaskRuntimeSummary();

const TaskStatus_t *findTaskStatus(const std::vector<TaskStatus_t> &tasks, UBaseType_t taskNumber);
uint64_t runtimeDelta(uint64_t currentValue, uint64_t previousValue);
float runtimePct(uint64_t runtimeValue, uint64_t totalRuntime);
float cpuLoadPct(const TaskRuntimeSnapshot &current, const TaskRuntimeSnapshot &previous);
float snapshotWindowSec(const TaskRuntimeSnapshot &current, const TaskRuntimeSnapshot &previous);
bool isIdleTaskName(const char *taskName);
const char *taskStateName(eTaskState state);
