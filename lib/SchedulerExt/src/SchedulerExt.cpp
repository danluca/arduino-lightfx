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

static const char *const unknown PROGMEM = "Thd N/R";

SchedulerClassExt Scheduler;

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
 * Starts a task with optional setup step and main loop
 * @param task the task structure - setup (optional, executed once) and main loop (required, executed indefinitely) callback pointers, stack and name info
 * @return the rtos thread pointer for the newly created thread that handles this task actions
 */
rtos::Thread* SchedulerClassExt::startTask(TasksPtr task) {
    uint i = findNextThreadSlot();
    if (i >= MAX_THREADS_NUMBER)
        return nullptr;

    threads[i] = task->threadName ? new ThreadWrapper(task->threadName, task->stackSize) : new ThreadWrapper(i, task->stackSize);
    threads[i]->getThread()->start(mbed::callback(setupLoopHelper, task));
    return threads[i]->getThread();
}

/**
 * Starts a new thread with main loop only, executed indefinitely
 * @param task main loop task
 * @param stackSize stack size in bytes to allocate for the task; default to 1024 if not specified
 * @return the new OS thread created
 */
rtos::Thread* SchedulerClassExt::startLoop(SchedulerTask task, uint32_t stackSize) {
    uint i = findNextThreadSlot();
    if (i >= MAX_THREADS_NUMBER)
        return nullptr;

    threads[i] = new ThreadWrapper(i, stackSize);
    threads[i]->getThread()->start(mbed::callback(loopHelper, task));
    return threads[i]->getThread();
}

/**
 * Starts a new thread that runs once a task
 * @param task the task
 * @param stackSize stack size in bytes to allocate for the task; default to 1024 if not specified
 * @return the new OS thread created
 * @see SchedulerClassExt::waitToEnd
 */
rtos::Thread* SchedulerClassExt::start(SchedulerTask task, uint32_t stackSize) {
    uint i = findNextThreadSlot();
    if (i >= MAX_THREADS_NUMBER)
        return nullptr;

    threads[i] = new ThreadWrapper(i, stackSize);
    threads[i]->getThread()->start(task);
    return threads[i]->getThread();
}

/**
 * Starts a new thread that runs once a parameterized task (one parameter of type pointer to any data type)
 * @param task the task
 * @param taskData argument to pass to the task
 * @param stackSize stack size in bytes to allocate for the task; default to 1024 if not specified
 * @return the new OS thread created
 * @see SchedulerClassExt::waitToEnd
 */
rtos::Thread* SchedulerClassExt::start(SchedulerParametricTask task, void *taskData, uint32_t stackSize) {
    uint i = findNextThreadSlot();
    if (i >= MAX_THREADS_NUMBER)
        return nullptr;

    threads[i] = new ThreadWrapper(i, stackSize);
    threads[i]->getThread()->start(mbed::callback(task, taskData));
    return threads[i]->getThread();
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
        if (thread->getThread() == pt) {
            //waits for thread to terminate
            tStat = thread->getThread()->join();
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
    thread->terminate();
    delete thread;
    delete name;
}


