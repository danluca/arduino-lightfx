///////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2023 (c) Dan Luca
///////////////////////////////////////////////////////////////////////////////////////////////////
// Collection of light strip effects with the ability to be configured through Wi-Fi
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <LittleFS.h>
#include <SchedulerExt.h>
#include <queue.h>
#include "filesystem.h"
#include "net_setup.h"
#include "efx_setup.h"
#include "sysinfo.h"
#include "diag.h"
#include "comms.h"
#include "FxSchedule.h"
#include "log.h"
#include "mic.h"
#include "util.h"
#include "web_server.h"
#include "ota_upgrade.h"

/**
 * TASK ALLOCATIONS
 * First Core
 *   - CORE0 (default task - setup, loop) - Web and communications, lowered priority (5)
 *   - ALM - alarm processing and some misc actions (inherited priority - 5)
 *   - FS - filesystem interaction (raised priority from calling task - 7)
 *   - IdleCore0, USB - default kernel tasks
 *
 * Second Core
 *   - CORE1 (default task - setup1, loop1) - Diag - diagnostic tasks, interaction with I2C devices, elevated priority (7)
 *   - FX - light effects (regular priority - 6)
 *   - Mic - microphone processing (regular priority - 6)
 *   - IdleCore1 - default kernel task
 *
 * Following kernel tasks are set to run on either core (core affinity 0xFFFFFFFF):
 *   - SRL - serial logging, enabled for either core, started from a Core0 task (regular priority - 6)
 *   - IDLE0, IDLE1, Tmr Svc
 * CORE0, CORE1 tasks are created with a default 1024 bytes of stack, fixed size
 * There seem to be this odd coupling between FX and Mic tasks, if both are enabled they need to be on the same core.
 * I've had success running both on either CORE0 or CORE1, but not on different cores.
 * On the other hand, the WiFi using WiFiNINA library seems to only work well on CORE0.
 */

void web_run();
void alarm_misc_begin();
void alarm_misc_run();
//task definitions for effects and mic processing - these tasks have the same priority as the main task, hence using 255 for priority value; see Scheduler.startTask
constexpr TaskDef fxTasks {fx_setup, fx_run, 1024, csFxTask, 255, CORE_1};
constexpr TaskDef micTasks {mic_setup, mic_run, 896, "Mic", 5, CORE_1};
constexpr TaskDef alarmTasks {alarm_misc_begin, alarm_misc_run, 1024, "ALM", 5, CORE_0};
bool core1_separate_stack = true;
QueueHandle_t almQueue;

/**
 * Sends the ALARM_SETUP message to the ALM task via the alarm queue.
 *
 * This function enqueues the ALARM_SETUP message into the `almQueue`, which is used for inter-task
 * communication regarding alarm-related actions. The function blocks for a maximum of 2000 milliseconds
 * while attempting to send the message - this is safe as this function does not execute in the context
 * of software timer callback.
 *
 * Behavior:
 * - Attempts to send the `ALARM_SETUP` message to the `almQueue` with a timeout of 2000 milliseconds.
 * - Logs an error if the message could not be enqueued.
 */
void enqueueAlarmSetup() {
    constexpr MiscAction msgSetup = ALARM_SETUP;
    if (const BaseType_t qResult = xQueueSend(almQueue, &msgSetup, 0); qResult != pdTRUE)
        log_error(F("Error sending ALARM_SETUP message to ALM queue - error %d"), qResult);
}

/**
 * ALM task begins - for now just blinking the board status LED while in setup mode
 */
void alarm_misc_begin() {
    state_led_begin();
    log_info(F("ALM task setup completed"));
}

/**
 * Miscellaneous & alarm task handler
 *
 * This function processes miscellaneous alarm-related actions received through a message queue.
 * Actions are specified using the `MiscAction` enum, and the function executes corresponding
 * behavior based on the received action type.
 *
 * Behavior:
 * - Executes the appropriate action based on the `MiscAction` received:
 *   - ALARM_SETUP: Calls `alarm_setup()` to initialize alarm settings.
 *   - ALARM_CHECK: Calls `alarm_check()` to verify current alarm status.
 *   - SAVE_SYS_INFO: Calls `saveSysInfo()` to persist system state information.
 *   - STATUS_LED_CHECK: Calls `state_led_run()` to update board status LED.
 * - Logs an error if an unsupported or unrecognized action is encountered.
 *
 * Notes:
 * - `almQueue` is used as the primary communication mechanism for this task.
 * - Requires the `MiscAction` enumeration for defining supported tasks.
 * - Relies on external functions, namely `alarm_setup`, `alarm_check`, and `saveSysInfo` for specific actions.
 * - Executes on the ALM task, typically assigned to CORE_1 in the system configuration.
 */
void alarm_misc_run() {
    MiscAction action;
    // wait indefinitely for a message to be received
    if (pdFALSE == xQueueReceive(almQueue, &action, portMAX_DELAY)) {
        return;
    }
    switch (action) {
        case ALARM_SETUP: alarm_setup(); break;
        case ALARM_CHECK: alarm_check(); break;
        case SAVE_SYS_INFO: saveSysInfo(); break;
        case HOLIDAY_UPDATE: holidayUpdate(); break;
        default:
            log_error(F("Misc Action %hu not supported"), action);
    }
}

/**
 * Executes the primary web-related tasks - runs the web server and communication functions
 * This function is intended to be invoked regularly to handle web communication and actions efficiently.
 */
void web_run() {
    web::webserver();
    commRun();
}

void filesystem_setup() {
    SyncFsImpl.begin(LittleFS);
    log_info(F("Filesystem setup completed"));
    sysInfo->setSysStatus(SYS_STATUS_FILESYSTEM);
}

//===First core tasks===
/**
 * Core 0 Setup LED strip and global data structures
 * NOTE: Core 0 task (setup and loop) is created with 1024 bytes stack memory - fixed value (see framework-arduinopico\libraries\FreeRTOS\src\variantHooks.cpp#startFreeRTOS)
 */
void setup() {
    taskDelay(2000);    //safety delay
    SysInfo::setupStateLED();
    log_setup();

    RP2040::enableDoubleResetBootloader();   //that's just a good idea overall

    sysInfo = new SysInfo();    //system information object built once per run
    filesystem_setup();
    sysInfo->begin();

    almQueue = xQueueCreate(10, sizeof(MiscAction));    //create a receiving queue for the ALM task for communication between cores
    Scheduler.startTask(&alarmTasks);

    readSysInfo();
    secElement_setup();

    const TaskHandle_t core1 = xTaskGetHandle(csCORE1);    //retrieve a task handle for the second core
    const BaseType_t c1Fx = xTaskNotify(core1, 1, eSetValueWithOverwrite);    //notify the second core that it can start running FX
    log_info(F("Basic components ok - CORE1 notified of starting FX %d. System status: %#hX"), c1Fx, sysInfo->getSysStatus());

    wifi_setup();
    taskDelay(2500);    //let the WiFi settle
    timeSetup();
    commSetup();
    web::server_setup();

    // notifies Core1 to start processing tasks that need WiFi
    const BaseType_t c1NtfStatus = xTaskNotify(core1, 2, eSetValueWithOverwrite);

    watchdogSetup();

    vTaskPrioritySet(nullptr, uxTaskPriorityGet(nullptr)-1);    //lower the priority of the main task to allow for other tasks to run
    taskDelay(250);         // leave reasonable time to the alarm task to set up
    //enqueues the alarm setup event if time is ok
    if (sysInfo->isSysStatus(SYS_STATUS_NTP))
        enqueueAlarmSetup();
    else
        log_warn(F("System time not yet synchronized with NTP, skipping alarm setup"));

    //wait for the other core to finish all initializations before allowing web server to respond to requests
    // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    sysInfo->setSysStatus(SYS_STATUS_SETUP0);
    log_info(F("Main CORE0 Setup completed, CORE1 notified of WiFi %d. System status: %#hX"), c1NtfStatus, sysInfo->getSysStatus());
    logSystemInfo();
}

/**
 * Core 0 Main loop - runs the web actions
 */
void loop() {
    web_run();
    handle_fw_upgrade();
}


//===Second core tasks===
/**
 * Core 1 Setup communication and diagnostic tasks
 * NOTE: Core 1 task (setup1 and loop1) is created with 1024 bytes stack memory - fixed value (see framework-arduinopico\libraries\FreeRTOS\src\variantHooks.cpp#startFreeRTOS)
 */
void setup1() {
    //wait for the main core to notify us that the core components are ready (filesystem, logging, secure element), not interested in the notification value
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    Scheduler.startTask(&fxTasks);
    // taskDelay(250);         // leave reasonable time to FX task to set-up

    //wait for the main core to notify us that WiFi is ready, not interested in the notification value
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    Scheduler.startTask(&micTasks);
    diagSetup();

    vTaskPrioritySet(nullptr, uxTaskPriorityGet(nullptr)+1);    //raise the priority of the diag task to allow uninterrupted I2C interactions
    taskDelay(250);    // safety delay after priority bump
    // const TaskHandle_t core0 = xTaskGetHandle(csCORE0);    //retrieve a task handle for the first core
    // const BaseType_t c0NtfStatus = xTaskNotify(core0, 1, eSetValueWithOverwrite);    //notify the first core that it can start running the web server
    sysInfo->setSysStatus(SYS_STATUS_SETUP1);
    log_info(F("Main CORE1 Setup completed. System status: %#hX"), sysInfo->getSysStatus());
}

/**
 * Core 1 Main loop - runs the communication tasks
 */
void loop1() {
    diagExecute();
}

/**
 * Log an event to the console when stack overflow is encountered
 * @param xTask task handle for task that exceeded stack
 * @param pcTaskName name of the task that exceeded stack
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    if (Serial)
        Serial.printf("Stack overflow in task %s [%p]\n", pcTaskName, xTask);
}
