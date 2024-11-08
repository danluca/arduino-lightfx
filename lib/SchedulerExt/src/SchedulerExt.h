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

#ifndef ARDUINO_LIGHTFX_SCHEDULEREXT_H
#define ARDUINO_LIGHTFX_SCHEDULEREXT_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

#define MAX_THREADS_NUMBER    10

extern "C" {
    typedef void (*NoArgTask)();
}

struct ThreadTasks {
    SchedulerTask setup{};          // setup function pointer (called once), may be null
    SchedulerTask loop{};           // loop function pointer (called repeatedly indefinitely), cannot be null
    const uint32_t stackSize {1024};      // stack size in bytes to allocate to the new thread (default 1024)
    const char* threadName {};      // custom thread name provided optionally; if not provided thread name is built generically using "Thd N" pattern
struct FxTasks {
    NoArgTask setup;
    NoArgTask loop;
};

extern "C" {
    typedef FxTasks* FxTasksPtr;
}

class ThreadWrapper {
public:
    ThreadWrapper(uint index, uint32_t stackSize);
    ThreadWrapper(const char* thName, uint32_t stackSize);
    inline rtos::Thread * getThread() const { return thread; }
    ~ThreadWrapper();
private:
    char* name;
    rtos::Thread* thread;
};

enum CoreAffinity {
    CORE_0 = 0x01,
    CORE_1 = 0x02,
    CORE_ALL = 0x03
};

class Runnable {
public:
    virtual void run() = 0;
    virtual void finish() = 0;
};

class TaskJob : Runnable {
public:
    explicit TaskJob(const char* pAbr, NoArgTask run, NoArgTask pre = nullptr, uint32_t szStack = 1024);

    virtual ~TaskJob() {
        delete [] id;
    }

    [[noreturn]] void run () override;

    [[nodiscard]] inline const char *getName() const {
        return pcTaskGetName(handle);
    }

    inline void finish() override {
        bIsFinished = true;
    }

    inline void setCoreAffinity(CoreAffinity ca) {
        coreAffinity = ca;
    }

    [[nodiscard]] inline CoreAffinity getCoreAffinity() const {
        return coreAffinity;
    }

protected:
    NoArgTask fnSetup, fnLoop;
    TaskHandle_t handle{};
    uint32_t stackSize;
    CoreAffinity coreAffinity;
    const char *id;
    bool bRun = true;
    bool bIsFinished = false;

    friend class SchedulerClassExt;
};

class SchedulerClassExt {
protected:
    [[nodiscard]] int16_t findNextThreadSlot() const;
public:
    SchedulerClassExt() = default;

    bool startTask(NoArgTask loop, NoArgTask setup = nullptr, uint32_t stackSize = 1024);

    bool startTask(FxTasksPtr taskDef, uint32_t stackSize = 1024);

    bool startTask(TaskJob *pTask);

    bool stopTask(TaskJob *pt);

    [[nodiscard]] uint16_t availableThreads() const;
    osStatus waitToEnd(rtos::Thread *pt);
    osStatus terminate(rtos::Thread *pt);

    uint availableThreads() const;

    static void yield() { ::yield(); };
private:
    ThreadWrapper *threads[MAX_THREADS_NUMBER] {nullptr};
    TaskJob *tasks[MAX_THREADS_NUMBER] = {};
    static bool scheduleTask(TaskJob *taskJob, uint16_t tPos);
};

extern SchedulerClassExt Scheduler;

#endif //ARDUINO_LIGHTFX_SCHEDULEREXT_H
