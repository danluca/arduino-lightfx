/**
 * Copyright (C) 2023 Dan Luca
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SchedulerExt.h"

static constexpr char fmtTaskName[] PROGMEM = "Tsk %d";

SchedulerClassExt Scheduler;

void taskJobExecutor(void *params) {
    // Upcast to Runnable* (public base) so run()/terminate() are accessible via their public Runnable declarations.
    // The downcast to TaskWrapper* first ensures correct pointer arithmetic with public inheritance.
    Runnable *tj = static_cast<TaskWrapper*>(params);
    tj->run();
    tj->terminate();
    vTaskDelete(nullptr);   // self-delete per FreeRTOS docs when the entry function returns
}

/**
 * Starts a task based on the definition provided. Definition includes functions to execute, name, priority, stack size, core affinity
 * @param taskDef
 * @return pointer to TaskWrapper created for this task
 */
TaskWrapper *SchedulerClassExt::startTask(const TaskDefPtr taskDef) {
    // Resolve effective priority before taking the mutex — uxTaskPriorityGet(nullptr) is safe to call anytime
    const uint8_t effectivePriority = taskDef->priority >= configMAX_PRIORITIES
        ? static_cast<uint8_t>(uxTaskPriorityGet(nullptr))
        : taskDef->priority;

    CoreMutex core_mutex(&mutex);
    auto *job = new TaskWrapper(taskDef, static_cast<int16_t>(tasks.size()), effectivePriority);
    if (!scheduleTask(job)) {
        delete job;
        return nullptr;
    }
    tasks.push_back(job);
    return job;
}

/**
 * Enters the task into the RTOS scheduler's scope - stack memory gets allocated and the task will now be scheduled
 * CPU time based on its priority
 * @param taskJob the task information including functions to execute
 * @return true if scheduling was successful
 */
bool SchedulerClassExt::scheduleTask(TaskWrapper *taskJob) {
    const BaseType_t result = xTaskCreateAffinitySet(taskJobExecutor, taskJob->id, taskJob->stackSize, taskJob,
                                               taskJob->priority, taskJob->coreAffinity, &(taskJob->handle));
    if (result == pdPASS) {
        TaskStatus_t taskStatus;
        vTaskGetInfo(taskJob->handle, &taskStatus, pdFALSE, eReady);
        taskJob->uid = taskStatus.xTaskNumber;
    }
    return result == pdPASS;
}

/**
 * Waits for the thread to terminate, then disposes it and frees its slot in the local thread array
 * If the thread is not one tracked in the local thread array, it returns false
 * @param pt pointer to the thread to terminate
 * @return whether the task termination and resource cleanup were successful
 */
bool SchedulerClassExt::stopTask(TaskWrapper *pt) {
    // Remove from deque while holding the mutex, then wait and delete without it
    TaskWrapper *found = nullptr;
    {
        CoreMutex core_mutex(&mutex);
        for (auto it = tasks.begin(); it != tasks.end(); ++it) {
            if (*it == pt) {
                found = *it;
                tasks.erase(it);
                break;
            }
        }
    }
    if (!found)
        return false;
    const bool tskEnd = found->waitToEnd();
    delete found;
    return tskEnd;
}

/**
 * Stop all tasks started by this scheduler (in the reverse order they have been started)
 * @param forced whether to forcefully terminate task (not wait) or signal the task to terminate and wait 1 second (default)
 */
void SchedulerClassExt::stopAllTasks(const bool forced) {
    // Drain the deque while holding the mutex, then process without it to avoid blocking other callers
    std::deque<TaskWrapper*> toStop;
    {
        CoreMutex core_mutex(&mutex);
        toStop.swap(tasks);
    }
    while (!toStop.empty()) {
        TaskWrapper *task = toStop.back();
        toStop.pop_back();
        if (task == nullptr)
            continue;
        if (forced)
            task->terminate();
        else
            (void)task->waitToEnd();
        delete task;
    }
}

/**
 * Suspends all tasks managed by this scheduler. Alternative to stopAllTasks that's less destructive -
 * it doesn't free up any resources allocated to the tasks
 * Note: this does not suspend any of the core tasks
 * Note: no API is provided to resume all tasks. Reboot the system.
 */
void SchedulerClassExt::suspendAllTasks() const {
    CoreMutex core_mutex(&mutex);
    for (auto & task : tasks) {
        if (task != nullptr)
            vTaskSuspend(task->handle);
    }
}

/**
 * Retrieves the task wrapper with given name
 * @param name task name to find
 * @return task with given name, nullptr is no task exists with the input name
 */
TaskWrapper *SchedulerClassExt::getTask(const char *name) const {
    CoreMutex core_mutex(&mutex);
    for (auto & task : tasks) {
        if (task != nullptr && strcmp(task->id, name) == 0)
            return task;
    }
    return nullptr;
}

/**
 * Retrieves the task wrapper at the given positional index in the internal deque.
 * Note: indices shift when tasks are removed via stopTask/stopAllTasks — not a stable identifier.
 * @param index positional index (0-based) in the tasks deque
 * @return the task at given index, or nullptr if out of range
 */
TaskWrapper *SchedulerClassExt::getTask(const uint index) const {
    CoreMutex core_mutex(&mutex);
    return index >= tasks.size() ? nullptr : tasks[index];
}

/**
 * Retrieves the task wrapper that matches task unique number (TaskStatus_t.xTaskNumber) in the system
 * @param uid task unique number to lookup
 * @return the task wrapper that matches a task with the uid provided, nullptr is none found
 */
TaskWrapper *SchedulerClassExt::getTask(const UBaseType_t uid) const {
    CoreMutex core_mutex(&mutex);
    for (auto &task : tasks) {
        if (task != nullptr && task->uid == uid)
            return task;
    }
    return nullptr;
}

// TaskWrapper
/**
 * Initialize a task wrapper from task definitions
 * @param taskDef definitions
 * @param x index in the Scheduler tasks array that this task will take
 * @param effectivePriority resolved priority (sentinel already substituted by startTask)
 */
TaskWrapper::TaskWrapper(const TaskDefPtr taskDef, const int16_t x, const uint8_t effectivePriority) :
    fnSetup(taskDef->setup), fnLoop(taskDef->loop), stackSize(taskDef->stackSize),
    coreAffinity(taskDef->core), priority(effectivePriority), index(x) {
    if (taskDef->threadName) {
        const size_t sz = strlen(taskDef->threadName);
        id = new char[sz + 1]();   //zero initialized array
        strncpy(id, taskDef->threadName, sz);
    } else {
        const size_t sz = snprintf(nullptr, 0, fmtTaskName, index);
        id = new char[sz + 1](); //zero initialized array
        snprintf(id, sz+1, fmtTaskName, index);
    }
}

/**
 * Executes the task. Sets state to TERMINATED before returning so waitToEnd() can detect clean exit.
 */
void TaskWrapper::run() {
    state = EXECUTING;
    if (fnSetup)
        fnSetup();
    if (fnLoop != nullptr) {
        while (!_shouldStop) {
            fnLoop();
            vTaskDelay(1);
        }
    }
    state = TERMINATED;
}

/**
 * Signals the task to stop and waits for it to exit cleanly, then force-deletes on timeout.
 * The timeout is rounded up to nearest 100ms. Default timeout is 1000ms = 1s.
 * Can be called from other threads.
 */
bool TaskWrapper::waitToEnd(const uint16_t msTimeOut) {
    _shouldStop = true;
    uint16_t nbrLoops = msTimeOut/100 + 1;      //ensure we have at least 1 loop as well as round up the timeout to nearest 100ms
    while (state != TERMINATED && nbrLoops > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        nbrLoops--;
    }
    // Only force-delete if the task did not self-terminate; otherwise it will self-delete via vTaskDelete(nullptr)
    if (state != TERMINATED)
        vTaskDelete(handle);
    return state == TERMINATED;
}

/**
 * Forcefully terminates the task immediately. No-op if already terminated.
 */
void TaskWrapper::terminate() {
    if (state == TERMINATED)
        return;
    _shouldStop = true;
    state = TERMINATED;
    vTaskDelete(handle);
}
