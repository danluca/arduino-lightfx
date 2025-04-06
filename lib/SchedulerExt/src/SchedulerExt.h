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
#include <deque>

extern "C" {
    typedef void (*NoArgTask)();
    typedef void (*VoidArgTask)(void *);
}

enum CoreAffinity:uint8_t {
    CORE_0 = 0x01,
    CORE_1 = 0x02,
    CORE_ALL = 0xFF
};

struct TaskDef {
    NoArgTask setup{};                  // setup function pointer (called once), may be null
    NoArgTask loop{};                   // loop function pointer (called repeatedly indefinitely), cannot be null
    const uint32_t stackSize {1024};    // stack size in bytes to allocate to the new thread (default 1024)
    const char* threadName {};          // custom thread name provided optionally; if not provided thread name is built generically using "Thd N" pattern
    mutable uint8_t priority {1};       // the task priority - must be between 1 and configMAX_PRIORITIES-1; the IDLE task has priority 0; mutable as the actual priority may be determined in relation with launching task
    CoreAffinity core {CORE_0};         // which core to run on - default to main core (0)
};

extern "C" {
    typedef const TaskDef* TaskDefPtr;
}

class Runnable {
public:
    virtual ~Runnable() = default;

    virtual void run() = 0;
    virtual void terminate() = 0;
    enum State:uint8_t {NEW, EXECUTING, TERMINATED};
};

class TaskWrapper final : Runnable {
public:
    explicit TaskWrapper(TaskDefPtr taskDef, int16_t x);

    ~TaskWrapper() override { delete [] id; }

    [[nodiscard]] const char *getName() const { return state == NEW ? id : pcTaskGetName(handle); }
    [[nodiscard]] uint getIndex() const { return index; }
    [[nodiscard]] CoreAffinity getCoreAffinity() const { return coreAffinity; }
    [[nodiscard]] TaskHandle_t getTaskHandle() const { return handle; }
    [[nodiscard]] uint32_t getStackSize() const { return stackSize; }
    [[nodiscard]] BaseType_t getPriority() const { return priority; }
    [[nodiscard]] UBaseType_t getUID() const { return uid; }
    [[nodiscard]] State getState() const { return state; }

protected:
    void run () override;
    [[nodiscard]] bool waitToEnd(uint16_t msTimeOut=1000) const;    //defaults to waiting 1s for task to finish
    void terminate() override;

    const NoArgTask fnSetup, fnLoop;
    TaskHandle_t handle{};
    const uint32_t stackSize;
    const CoreAffinity coreAffinity;
    const BaseType_t priority;
    char *id;
    const int16_t index;
    UBaseType_t uid{};
    volatile Runnable::State state {NEW};
    friend class SchedulerClassExt;
};

class SchedulerClassExt {
public:
    SchedulerClassExt() = default;

    TaskWrapper* startTask(TaskDefPtr taskDef);

    bool stopTask(const TaskWrapper *pt);
    [[nodiscard]] TaskWrapper* getTask(uint index) const;
    [[nodiscard]] TaskWrapper* getTask(const char* name) const;
    [[nodiscard]] TaskWrapper* getTask(UBaseType_t uid) const;
    void stopAllTasks(bool forced = false);
    void suspendAllTasks() const;

    static void yield() { ::yield(); };
    static void delay(const uint32_t ms) { ::vTaskDelay(pdMS_TO_TICKS(ms)); };
private:
    std::deque<TaskWrapper*> tasks {};
    static bool scheduleTask(TaskWrapper *taskJob);
};

extern SchedulerClassExt Scheduler;

#endif //ARDUINO_LIGHTFX_SCHEDULEREXT_H
