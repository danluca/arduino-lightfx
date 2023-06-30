/**
 * Copyright (C) 2012 The Android Open Source Project
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

SchedulerClassExt::SchedulerClassExt() = default;

/**
 * Method that implements optional setup + continuous run of the task
 * Do not use this method if not intending to run a loop
 * @param args tasks to run in the new thread - the setup function pointer is optional, the loop function pointer is required
 */
[[noreturn]] static void setupLoopHelper(TasksPtr args) {
    if (args->setup != nullptr)
        args->setup();
    while (true) {
        args->loop();
    }
}

/**
 * Overload that just runs a task in continuous loop
 * @param loopTask the task to run
 */
[[noreturn]] static void loopHelper(SchedulerTask loopTask) {
    while (true) {
        loopTask();
    }
}

/**
 * Starts a task with setup (optional) and main loop
 * @param tasks optional setup and (required) main loop
 * @param stackSize thread's stack size - defaults to 1024 if not specified
 * @return
 */
rtos::Thread* SchedulerClassExt::startLoop(TasksPtr tasks, uint32_t stackSize) {
    uint i = findNextThreadSlot();
    if (i >= MAX_THREADS_NUMBER)
        return nullptr;

    const char *tName = createThreadName(i);
    threads[i] = new rtos::Thread(osPriorityNormal, stackSize, nullptr, tName);
    threads[i]->start(mbed::callback(setupLoopHelper, tasks));
    return threads[i];
}

rtos::Thread* SchedulerClassExt::startLoop(SchedulerTask task, uint32_t stackSize) {
    uint i = findNextThreadSlot();
    if (i >= MAX_THREADS_NUMBER)
        return nullptr;

    const char *tName = createThreadName(i);
    threads[i] = new rtos::Thread(osPriorityNormal, stackSize, nullptr, tName);
    threads[i]->start(mbed::callback(loopHelper, task));
    return threads[i];
}

rtos::Thread* SchedulerClassExt::start(SchedulerTask task, uint32_t stackSize) {
    uint i = findNextThreadSlot();
    if (i >= MAX_THREADS_NUMBER)
        return nullptr;

    const char *tName = createThreadName(i);
    threads[i] = new rtos::Thread(osPriorityNormal, stackSize, nullptr, tName);
    threads[i]->start(task);
    return threads[i];
}

rtos::Thread* SchedulerClassExt::start(SchedulerParametricTask task, void *taskData, uint32_t stackSize) {
    uint i = findNextThreadSlot();
    if (i >= MAX_THREADS_NUMBER)
        return nullptr;

    const char *tName = createThreadName(i);
    threads[i] = new rtos::Thread(osPriorityNormal, stackSize, nullptr, tName);
    threads[i]->start(mbed::callback(task, taskData));
    return threads[i];
}

/**
 * Waits for the thread to terminate, then disposes it and frees its slot in the local thread array
 * If the thread is not one tracked in the local thread array, it returns osErrorParameter
 * @param pt pointer to the thread to terminate
 * @return result of executing Thread::join for the given thread
 */
osStatus SchedulerClassExt::waitToEnd(rtos::Thread *pt) {
    osStatus tStat = osErrorParameter;
    for (auto & thread : threads) {
        if (thread == pt) {
            //waits for thread to terminate
            tStat = thread->join();
            //deallocate the thread (created with new) and its name
            delete [] thread->get_name();
            delete thread;
            //free-up the thread array spot for it
            thread = nullptr;
            break;
        }
    }
    return tStat;
}

/**
 * Finds the index for the next thread.
 * If the local thread array is full, it returns osThreadSpaceExhausted
 * @return either the index where next thread can be stored in the local thread array, or osThreadSpaceExhausted (0xF0F0)
 */
uint SchedulerClassExt::findNextThreadSlot() const {
    uint i = 0;
    while (threads[i] != nullptr && i < MAX_THREADS_NUMBER)
        i++;

    return i < MAX_THREADS_NUMBER ? i : osThreadSpaceExhausted;
}

/**
 * Utility to create a simple thread name - using pattern Thread <n>, where n is the index in the local thread array
 * Note this method creates the string on the heap, the thread name must be available for the life of the thread. When thread is disposed of,
 * the memory allocated for name needs also released.
 * @param index the index to create name for (0 based)
 * @return pointer to the thread name string on the heap
 * @see SchedulerClassExt::waitToEnd
 */
const char *SchedulerClassExt::createThreadName(uint index) {
    char* tName = new char[MAX_THREAD_NAME_SIZE] {};
    sprintf(tName, "Thread %d", index);
    return tName;
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

SchedulerClassExt Scheduler;

