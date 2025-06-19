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

#define TASK_NOTIFY_TERMINATE 0xF0

static constexpr char fmtTaskName[] PROGMEM = "Tsk %d";

SchedulerClassExt Scheduler;

void taskJobExecutor(void *params) {
    auto *tj = static_cast<Runnable*>(params);
    tj->run();
    //should never get here - but if we do, must delete the task per FreeRTOS documentation - https://www.freertos.org/implementing-a-FreeRTOS-task.html
    tj->terminate();
}

/**
 * Starts a task based on the definition provided. Definition includes functions to execute, name, priority, stack size, core affinity
 * @param taskDef
 * @return pointer to TaskWrapper created for this task
 */
TaskWrapper *SchedulerClassExt::startTask(const TaskDefPtr taskDef) {
    CoreMutex core_mutex(&mutex);
    //if priority is not provided (above the range), use the default priority of the calling task
    if (taskDef->priority >= configMAX_PRIORITIES)
        taskDef->priority = uxTaskPriorityGet(nullptr);
    auto *job = new TaskWrapper(taskDef, tasks.size());
    tasks.push_back(job);
    return scheduleTask(job) ? job : nullptr;
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
 * If the thread is not one tracked in the local thread array, it returns osErrorParameter
 * @param pt pointer to the thread to terminate
 * @return whether the task termination and resource cleanup were successful
 */
bool SchedulerClassExt::stopTask(const TaskWrapper *pt) {
    CoreMutex core_mutex(&mutex);
    for (auto it = tasks.begin(); it != tasks.end(); ++it) {
        if (const auto *task = *it; task == pt) {
            //signal task to terminate and wait
            const bool tskEnd = task->waitToEnd();
            //deallocate the thread (created with new) and its name
            tasks.erase(it);
            delete task;
            return tskEnd;
        }
    }
    return false;
}

/**
 * Stop all tasks started by this scheduler (in the reverse order they have been started)
 * @param forced whether to forcefully terminate task (not wait) or signal the task to terminate and wait 1 second (default)
 */
void SchedulerClassExt::stopAllTasks(const bool forced) {
    CoreMutex core_mutex(&mutex);
    //iterate tasks in reverse order and stop them
    while (tasks.size() > 0) {
        if (TaskWrapper *task = tasks.back(); task != nullptr) {
            if (forced)
                task->terminate();          //forcefully terminate task
            else
                (void)task->waitToEnd();    //signal task to terminate and wait
            //free up resources
            tasks.pop_back();
            delete task;
        }
    }
}

/**
 * Suspends all tasks managed by this scheduler. Alternative to stopAllTasks that's less destructive -
 * it doesn't free up any resources allocated to the tasks
 * Note: this does not suspend any of the core tasks
 * Note: no API is provided to resume all tasks. Reboot the system.
 */
void SchedulerClassExt::suspendAllTasks() const {
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
    for (auto & task : tasks) {
        if (task != nullptr && strcmp(task->id, name) == 0)
            return task;
    }
    return nullptr;
}

/**
 * Retrieves the task wrapper at given index
 * @param index task index to retrieve
 * @return the task at given index, or nullptr if the index is higher than max tasks or there is no task at the index
 */
TaskWrapper *SchedulerClassExt::getTask(const uint index) const {
    return index >= tasks.size() ? nullptr : tasks[index];
}

/**
 * Retrieves the task wrapper that matches task unique number (TaskStatus_t.xTaskNumber) in the system
 * @param uid task unique number to lookup
 * @return the task wrapper that matches a task with the uid provided, nullptr is none found
 */
TaskWrapper *SchedulerClassExt::getTask(const UBaseType_t uid) const {
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
 */
TaskWrapper::TaskWrapper(const TaskDefPtr taskDef, const int16_t x) : fnSetup(taskDef->setup), fnLoop(taskDef->loop), stackSize(taskDef->stackSize),
    coreAffinity(taskDef->core), priority(taskDef->priority), index(x) {
    if (taskDef->threadName) {
        const size_t sz = strlen(taskDef->threadName);
        id = new char[sz + 1]();   //zero initialized array
        strncpy(id, taskDef->threadName, sz);
    } else {
        const size_t sz = sprintf(nullptr, fmtTaskName, index);
        id = new char[sz + 1](); //zero initialized array
        snprintf(id, sz, fmtTaskName, index);
    }
}

/**
 * Executes the task
 */
void TaskWrapper::run() {
    state = EXECUTING;
    if (fnSetup)
        fnSetup();
    if (fnLoop == nullptr)
        return;
    //with each loop, check if we have been notified to stop - it is important to block for at least 1 tick
    //such that the task scheduler can give other threads a chance to run
    while (ulTaskNotifyTake(pdTRUE, 1) != TASK_NOTIFY_TERMINATE)
        fnLoop();
}

/**
 * Notifies the task to stop running. Returns when the notification was received, fnLoop execution finished or the timeout expired, and task was deleted.
 * The timeout is rounded up to nearest 100ms. Default timeout is 1000ms = 1s.
 * Can be called from other threads
 */
bool TaskWrapper::waitToEnd(const uint16_t msTimeOut) const {
    xTaskNotify(handle, TASK_NOTIFY_TERMINATE, eSetValueWithOverwrite);
    //wait for the task to finish a fnLoop execution or timeout
    uint16_t nbrLoops = msTimeOut/100 + 1;      //ensure we have at least 1 loop as well as round up the timeout to nearest 100ms
    while (state != TERMINATED && nbrLoops > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        nbrLoops--;
    }
    vTaskDelete(handle);
    return state == TERMINATED;
}

/**
 * Forcefully terminates the task
 */
void TaskWrapper::terminate() {
    if (state < EXECUTING)
        return;
    state = TERMINATED;
    vTaskDelete(handle);
}

