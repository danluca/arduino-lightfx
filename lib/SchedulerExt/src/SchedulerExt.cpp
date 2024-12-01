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

static const char fmtTaskName[] PROGMEM = "Thd %d";

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
TaskWrapper *SchedulerClassExt::startTask(TaskDefPtr taskDef) {
    int16_t tPos = findNextThreadSlot();
    if (tPos < 0)
        return nullptr;
    auto *job = new TaskWrapper(taskDef, tPos);
    tasks[tPos] = job;
    return scheduleTask(job) ? job : nullptr;
}

/**
 * How many slots do we have available in the local threads array
 * @return number of available threads, if any
 */
uint16_t SchedulerClassExt::availableThreads() const {
    uint16_t res = 0;
    for (auto task : tasks) {
        if (task == nullptr)
            res++;
    }
    return res;
}

/**
 * Enters the task into the RTOS scheduler's scope - stack memory gets allocated and the task will now be scheduled
 * CPU time based on its priority
 * @param taskJob the task information including functions to execute
 * @return true if scheduling was successful
 */
bool SchedulerClassExt::scheduleTask(TaskWrapper *taskJob) {
    BaseType_t result = xTaskCreateAffinitySet(taskJobExecutor, taskJob->id, taskJob->stackSize, taskJob,
                                               taskJob->priority, taskJob->coreAffinity, &(taskJob->handle));
    return result == pdPASS;
}

/**
 * Finds the index for the next thread.
 * If the local thread array is full, it returns -1
 * @return either the index where next thread can be stored in the local thread array, or -1 (0xFFFF)
 */
int16_t SchedulerClassExt::findNextThreadSlot() const {
    for (int16_t i = 0; i < MAX_THREADS_NUMBER; i++)
        if (tasks[i] == nullptr)
            return i;
    return -1;
}

/**
 * Waits for the thread to terminate, then disposes it and frees its slot in the local thread array
 * If the thread is not one tracked in the local thread array, it returns osErrorParameter
 * @param pt pointer to the thread to terminate
 * @return result of executing Thread::join for the given thread
 */
bool SchedulerClassExt::stopTask(TaskWrapper *pt) {
    bool result = false;
    for (auto & task : tasks) {
        if (task == pt) {
            //signal task to terminate - see TaskJob::run and taskJobExecutor
            task->waitToEnd();
            //deallocate the thread (created with new) and its name
            delete task;
            //free-up the thread array spot for it
            task = nullptr;
            result = true;
            break;
        }
    }
    return result;
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
TaskWrapper *SchedulerClassExt::getTask(uint index) const {
    return index >= MAX_THREADS_NUMBER ? nullptr : tasks[index];
}

/**
 * Retrieves the task wrapper that matches task unique number (TaskStatus_t.xTaskNumber) in the system
 * @param uid task unique number to lookup
 * @return the task wrapper that matches a task with the uid provided, nullptr is none found
 */
TaskWrapper *SchedulerClassExt::getTask(UBaseType_t uid) const {
    for (auto &task : tasks) {
        if (task != nullptr && uxTaskGetTaskNumber(task->handle) == uid)
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
TaskWrapper::TaskWrapper(TaskDefPtr taskDef, int16_t x) : fnSetup(taskDef->setup), fnLoop(taskDef->loop), stackSize(taskDef->stackSize), coreAffinity(taskDef->core),
                                                          priority(taskDef->priority), index(x), state(NEW) {
    if (taskDef->threadName) {
        size_t sz = strlen(taskDef->threadName);
        id = new char[sz + 1]();   //zero initialized array
        strcpy(id, taskDef->threadName);
    } else {
        size_t sz = sprintf(nullptr, fmtTaskName, index);
        id = new char[sz + 1](); //zero initialized array
        sprintf(id, fmtTaskName, index);
    }
}

/**
 * Executes the task
 */
void TaskWrapper::run() {
    state = EXECUTING;
    if (fnSetup)
        fnSetup();
    //with each loop, check if we have been notified to stop
    while (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1)) == 0U)
        fnLoop();
    state = TERMINATED;
}

/**
 * Notifies the task to stop running. Returns when the notification was received and fnLoop execution finished, or the timeout expired
 * The timeout is rounded up to nearest 100ms. Default timeout is 1000ms = 1s.
 */
bool TaskWrapper::waitToEnd(uint16_t msTimeOut) {
    xTaskNotify(handle, 1, eIncrement);
    //wait for the task to finish a fnLoop execution or timeout
    uint16_t nbrLoops = msTimeOut/100 + 1;      //ensure we have at least 1 loop as well as round up the timeout to nearest 100ms
    while (state != TERMINATED && nbrLoops > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        nbrLoops--;
    }
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

