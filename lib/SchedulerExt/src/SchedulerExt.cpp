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

static const char *const unknown PROGMEM = "Thd N/R";
static const char untitledTask[] = "UN";

void taskJobExecutor(void *params) {
    auto *tj = static_cast<Runnable*>(params);
    tj->run();
    //should never get here - but if we do, must delete the task per FreeRTOS documentation - https://www.freertos.org/implementing-a-FreeRTOS-task.html
    vTaskDelete(nullptr);
    tj->finish();
}

SchedulerClassExt::SchedulerClassExt() = default;

bool SchedulerClassExt::startTask(NoArgTask loop, NoArgTask setup, uint32_t stackSize) {
    int16_t tPos = findNextThreadSlot();
    if (tPos < 0)
        return false;
    auto *job = new TaskJob(untitledTask, loop, setup, stackSize);
    tasks[tPos] = job;
    return scheduleTask(job, tPos);
}

bool SchedulerClassExt::startTask(FxTasksPtr taskDef, uint32_t stackSize) {
    int16_t tPos = findNextThreadSlot();
    if (tPos < 0)
        return false;
    auto *job = new TaskJob(untitledTask, taskDef->loop, taskDef->setup, stackSize);
    tasks[tPos] = job;
    return scheduleTask(job, tPos);
}

bool SchedulerClassExt::startTask(TaskJob *pTask) {
    int16_t tPos = findNextThreadSlot();
    if (tPos < 0)
        return false;
    tasks[tPos] = pTask;
    return scheduleTask(pTask, tPos);
}

uint16_t SchedulerClassExt::availableThreads() const {
    uint16_t res = 0;
    for (auto task : tasks) {
        if (task == nullptr)
            res++;
    }
    return res;
}

bool SchedulerClassExt::scheduleTask(TaskJob *taskJob, uint16_t tPos) {
    char* tName = new char[configMAX_TASK_NAME_LEN] {};
    snprintf(tName, configMAX_TASK_NAME_LEN-1,  "TK%d %s", tPos, taskJob->id);
    BaseType_t result = xTaskCreateAffinitySet(taskJobExecutor, tName, taskJob->stackSize, taskJob, (UBaseType_t)4, taskJob->coreAffinity, &(taskJob->handle));
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
bool SchedulerClassExt::stopTask(TaskJob *pt) {
    bool result = false;
    for (auto & task : tasks) {
        if (task == pt) {
            //signal task to terminate - see TaskJob::run and taskJobExecutor
            task->bRun = false;
            //waits for thread to terminate
            while (!task->bIsFinished)
                yield();
            //deallocate the thread (created with new) and its name
            delete [] task->id;
            delete task;
            //free-up the thread array spot for it
            task = nullptr;
            result = true;
            break;
        }
    }
    return result;
}

SchedulerClassExt Scheduler;

TaskJob::TaskJob(const char *pAbr, NoArgTask run, NoArgTask pre, uint32_t szStack) : fnSetup(pre), fnLoop(run), stackSize(szStack), coreAffinity(CORE_ALL) {
    size_t szAbr = strlen(pAbr);
    id = new char[4] {
            szAbr > 0 ? *pAbr : '\0',
            szAbr > 1 ? *(pAbr+1) : '\0',
            szAbr > 2 ? *(pAbr+2) : '\0',
            '\0'
    };
}

void TaskJob::run() {
    if (fnSetup)
        fnSetup();
    while (bRun) {
        fnLoop();
    }
}

/**
 * How many slots do we have available in the local threads array
 * @return number of available threads, if any
 */
uint SchedulerClassExt::availableThreads() const {
    uint i = 0;
    while (threads[i] == nullptr && i < MAX_THREADS_NUMBER)
        i++;
    //the index is 0 based, we need to return how many slots are available
    return i;
}

//ThreadWrapper
/**
 * Creates a thread using the stack size and name determined as below.<br/>
 * Creates a simple thread name - using pattern Thd <n>, where n is the index in the local thread array
 * Note this method creates the string on the heap, the thread name must be available for the life of the thread. When thread is disposed of,
 * the memory allocated for name needs also released.
 * @param index index to use in the thread name
 * @param stackSize stack size to allocate to new thread, in bytes
 */
ThreadWrapper::ThreadWrapper(const uint index, const uint32_t stackSize) {
    size_t sz = sprintf(nullptr, "Thd %d", index);
    name = new char[sz+1](); //zero initialized array
    sprintf(name, "Thd %d", index);
    thread = new rtos::Thread(osPriorityNormal, stackSize, nullptr, name);
}

/**
 * Creates a thread using the stack size and name determined as below. <br/>
 * Uses the name provided for the thread name
 * @param thName thread name
 * @param stackSize stack size to allocate to new thread, in bytes
 */
ThreadWrapper::ThreadWrapper(const char *thName, uint32_t stackSize) {
    if (thName != nullptr) {
        size_t sz = strlen(thName);
        name = new char[sz + 1]();    //zero initialized array
        strcpy(name, thName);
        thread = new rtos::Thread(osPriorityNormal, stackSize, nullptr, name);
    } else {
        name = nullptr;
        thread = new rtos::Thread(osPriorityNormal, stackSize, nullptr, unknown);
    }
}

/**
 * Frees up the resources allocated on the heap - name and thread. Ensures the thread has been terminated
 */
ThreadWrapper::~ThreadWrapper() {
    delete thread;
    delete name;
}


