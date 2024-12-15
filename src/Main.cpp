///////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2023 (c) Dan Luca
///////////////////////////////////////////////////////////////////////////////////////////////////
// Collection of light strip effects with ability to be configured through Wi-Fi
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <SchedulerExt.h>
#include "filesystem.h"
#include "net_setup.h"
#include "efx_setup.h"
#include "sysinfo.h"
#include "diag.h"
#include "comms.h"
#include "FxSchedule.h"
#include "ledstate.h"
#include "log.h"
#include "mic.h"
#include "util.h"
#include <queue.h>

//task definitions for effects and mic processing - these tasks have the same priority as the main task, hence using 255 for priority value; see Scheduler.startTask
constexpr TaskDef fxTasks {fx_setup, fx_run, 3072, "Fx", 255, CORE_0};
constexpr TaskDef micTasks {mic_setup, mic_run, 896, "Mic", 255, CORE_0};
bool core1_separate_stack = true;
QueueHandle_t core0Queue;
QueueHandle_t core1Queue;

/**
 * Core 0 Setup LED strip and global data structures
 */
void setup() {
    taskDelay(2000);    //safety delay
    vTaskPrioritySet(nullptr, uxTaskPriorityGet(nullptr)-1);    //lower the priority of the main task to allow for other tasks to run
    setupStateLED();
    log_setup();

    stateLED(CLR_SETUP_IN_PROGRESS);    //Setup in progress
    RP2040::enableDoubleResetBootloader();   //that's just good idea overall

    sysInfo = new SysInfo();    //system information object built once per run
    fsSetup();

    readSysInfo();
    secElement_setup();

    Scheduler.startTask(&fxTasks);
    Scheduler.startTask(&micTasks);

    taskDelay(2000);    // leave reasonable time to FX and mic tasks to setup, doesn't really matter though

    stateLED(CLR_SETUP_IN_PROGRESS);    //Setup in progress

    watchdogSetup();

    core0Queue = xQueueCreate(10, sizeof(MiscAction));    //create a receiving queue for CORE0 task for communication between cores
    // notifies Core1 to start processing
    TaskHandle_t core1 = xTaskGetHandle("CORE1");    //create a task handle for the second core
    BaseType_t c1NtfStatus = xTaskNotify(core1, 1, eSetValueWithOverwrite);    //notify the second core that it can start running
    Log.info(F("Main Core 0 Setup completed, Core 1 notified %d. System status: %hX"), c1NtfStatus, sysInfo->getSysStatus());
}

/**
 * Core 0 Main loop - runs the miscellaneous actions
 */
void loop() {
    MiscAction action;
    // wait indefinitely for a message to be received
    if (pdFALSE == xQueueReceive(core0Queue, &action, portMAX_DELAY)) {
        vTaskDelay(pdMS_TO_TICKS(1000));   //wait a bit before trying again
        return;
    }
    switch (action) {
        case ALARM_SETUP: alarm_setup(); break;
        case ALARM_CHECK: alarm_check(); break;
        case SAVE_SYS_INFO: saveSysInfo(); break;
        default:
            Log.error(F("Misc Action %hhu not supported"), action);
    }
//    watchdogPing();   //the main functionality is in Fx thread, we can afford web server not being available
}

void enqueueAlarmSetup() {
    constexpr MiscAction msg = ALARM_SETUP;
    if (const BaseType_t qResult = xQueueSend(core0Queue, &msg, pdMS_TO_TICKS(2000)); qResult != pdTRUE)
        Log.error(F("Error sending ALARM_SETUP message to CORE0 task - error %d"), qResult);
}

//===Second core tasks===
/**
 * Core 1 Setup communication and diagnostic tasks
 */
void setup1() {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);    //wait for the main core to notify us that it's ready to run these tasks
    core1Queue = xQueueCreate(10, sizeof(CommAction));    //create a receiving queue for CORE1 task for communication between cores
    bool bSetupOk = wifi_setup();
    bSetupOk = bSetupOk && timeSetup();
    stateLED(bSetupOk ? CLR_ALL_OK : CLR_SETUP_ERROR);

    diagSetup();

    commSetup();

    //enqueues the alarm setup event
    enqueueAlarmSetup();

    Log.info(F("Main Core 1 Setup completed. System status: %hX"), sysInfo->getSysStatus());
    logSystemInfo();
}

/**
 * Core 1 Main loop - runs the communication tasks
 */
void loop1() {
    webserver();
    commRun();
    //check for any additional actions to be performed
    CommAction action;
    if (pdTRUE == xQueueReceive(core1Queue, &action, 0)) {
        switch (action) {
            case WIFI_ENSURE: wifi_ensure(); break;
            default:
                Log.error(F("Comm Action %hhd not supported"), action);
        }
    }
}

/**
 * Log an event to the console when stack overflow is encountered
 * @param xTask task handle for task that exceeded stack
 * @param pcTaskName name of the task that exceeded stack
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    Serial.printf("Stack overflow in task %s [%p]\n", pcTaskName, xTask);
}
