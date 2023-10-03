/**
 * Copyright (C) 2012 The Android Open Source Project
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
#include "mbed.h"

#define MAX_THREADS_NUMBER    10
#define MAX_THREAD_NAME_SIZE  10

#define osThreadSpaceExhausted  0xF0F0

extern "C" {
    typedef void (*SchedulerTask)(void);
    typedef void (*SchedulerParametricTask)(void *);
}

struct ThreadTasks {
    SchedulerTask setup;
    SchedulerTask loop;
};

extern "C" {
    typedef ThreadTasks* TasksPtr;
}

class SchedulerClassExt {
protected:
    uint findNextThreadSlot() const;
    static const char* createThreadName(uint index) ;
public:
    SchedulerClassExt();

    rtos::Thread* startLoop(SchedulerTask loopTask, uint32_t stackSize = 1024);

    rtos::Thread* startLoop(TasksPtr tasks, uint32_t stackSize = 1024);

    rtos::Thread* start(SchedulerTask task, uint32_t stackSize = 1024);

    rtos::Thread* start(SchedulerParametricTask task, void *data, uint32_t stackSize = 1024);

    osStatus waitToEnd(rtos::Thread *pt);

    uint availableThreads() const;

    void yield() { ::yield(); };
private:
    rtos::Thread *threads[MAX_THREADS_NUMBER] = {nullptr};
};

extern SchedulerClassExt Scheduler;

#endif //ARDUINO_LIGHTFX_SCHEDULEREXT_H
