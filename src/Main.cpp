///////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2023 (c) Dan Luca
///////////////////////////////////////////////////////////////////////////////////////////////////
// Collection of light strip effects with ability to be configured through Wi-Fi
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
#include "ledstate.h"
#include "log.h"
#include "mic.h"
#include "util.h"
#include "web_server.h"

/**
 * TASK ALLOCATIONS
 * First Core
 *   - CORE0 (default task - setup, loop) - Web and communications, lowered priority (5)
 *   - ALM - alarm processing and some misc actions (inherited priority - 5)
 *   - FS - filesystem interaction (inherited priority - 5)
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
void alarm_misc_run();
void enqueueAlarmSetup();
//task definitions for effects and mic processing - these tasks have the same priority as the main task, hence using 255 for priority value; see Scheduler.startTask
constexpr TaskDef fxTasks {fx_setup, fx_run, 2048, "Fx", 255, CORE_1};
constexpr TaskDef micTasks {mic_setup, mic_run, 896, "Mic", 255, CORE_1};
constexpr TaskDef alarmTasks {nullptr, alarm_misc_run, 1024, "ALM", 255, CORE_0};
bool core1_separate_stack = true;
QueueHandle_t almQueue;
QueueHandle_t webQueue;

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
    constexpr MiscAction msg = ALARM_SETUP;
    if (const BaseType_t qResult = xQueueSend(almQueue, &msg, pdMS_TO_TICKS(2000)); qResult != pdTRUE)
        Log.error(F("Error sending ALARM_SETUP message to CORE0 task - error %d"), qResult);
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
        vTaskDelay(pdMS_TO_TICKS(1000));   //wait a bit before trying again
        return;
    }
    switch (action) {
        case ALARM_SETUP: alarm_setup(); break;
        case ALARM_CHECK: alarm_check(); break;
        case SAVE_SYS_INFO: saveSysInfo(); break;
        default:
            Log.error(F("Misc Action %hu not supported"), action);
    }
}

/**
 * Executes the primary web-related tasks.
 *
 * - Runs the web server and communication functions
 * - Checks for any additional queued actions by receiving messages from `webQueue`:
 *   - If the action is WIFI_ENSURE, it ensures WiFi is properly functioning by calling wifi_ensure().
 *   - Logs an error for unsupported or unrecognized actions.
 *
 * This function is intended to be invoked regularly to handle web communication and actions efficiently.
 */
void web_run() {
    web::webserver();
    commRun();
    //check for any additional actions to be performed
    CommAction action;
    if (pdTRUE == xQueueReceive(webQueue, &action, 0)) {
        switch (action) {
            case WIFI_ENSURE: wifi_ensure(); break;
            default:
                Log.error(F("Comm Action %hu not supported"), action);
        }
    }
}

void filesystem_setup() {
    SyncFsImpl.begin(LittleFS);
    Log.info(F("Filesystem setup completed"));
    sysInfo->setSysStatus(SYS_STATUS_FILESYSTEM);
}

//===First core tasks===
/**
 * Core 0 Setup LED strip and global data structures
 * NOTE: Core 0 task (setup and loop) is created with 1024 bytes stack memory - fixed value (see framework-arduinopico\libraries\FreeRTOS\src\variantHooks.cpp#startFreeRTOS)
 */
void setup() {
    taskDelay(2000);    //safety delay
    vTaskPrioritySet(nullptr, uxTaskPriorityGet(nullptr)-1);    //lower the priority of the main task to allow for other tasks to run
    setupStateLED();
    log_setup();

    stateLED(CLR_SETUP_IN_PROGRESS);    //Setup in progress
    RP2040::enableDoubleResetBootloader();   //that's just good idea overall

    sysInfo = new SysInfo();    //system information object built once per run
    filesystem_setup();

    readSysInfo();
    secElement_setup();

    const TaskHandle_t core1 = xTaskGetHandle("CORE1");    //retrieve a task handle for the second core
    const BaseType_t c1Fx = xTaskNotify(core1, 1, eSetValueWithOverwrite);    //notify the second core that it can start running FX
    Log.info(F("Basic components ok - Core 1 notified of starting FX %d. System status: %#hX"), c1Fx, sysInfo->getSysStatus());

    webQueue = xQueueCreate(10, sizeof(CommAction));    //create a receiving queue for CORE0 task for communication between cores
    almQueue = xQueueCreate(10, sizeof(MiscAction));    //create a receiving queue for ALM task for communication between cores
    bool bSetupOk = wifi_setup();
    taskDelay(2500);    //let the WiFi settle
    bSetupOk = bSetupOk && timeSetup();
    stateLED(bSetupOk ? CLR_ALL_OK : CLR_SETUP_ERROR);
    commSetup();

    // notifies Core1 to start processing tasks that need WiFi
    const BaseType_t c1NtfStatus = xTaskNotify(core1, 2, eSetValueWithOverwrite);

    Scheduler.startTask(&alarmTasks);
    taskDelay(250);         // leave reasonable time to alarm task to setup
    //enqueues the alarm setup event
    enqueueAlarmSetup();

    //wait for the other core to finish all initializations before allowing web server to respond to requests
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    web::server_setup();

    watchdogSetup();
    Log.info(F("Main Core 0 Setup completed, CORE1 notified of WiFi %d. System status: %#hX"), c1NtfStatus, sysInfo->getSysStatus());
    logSystemInfo();
}

/**
 * Core 0 Main loop - runs the web actions
 */
void loop() {
    web_run();
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
    taskDelay(250);         // leave reasonable time to FX task to setup
    Scheduler.startTask(&micTasks);

    //wait for the main core to notify us that WiFi is ready, not interested in the notification value
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    vTaskPrioritySet(nullptr, uxTaskPriorityGet(nullptr)+1);    //raise the priority of the diag task to allow uninterrupted I2C interactions
    taskDelay(250);    // safety delay after priority bump
    diagSetup();

    const TaskHandle_t core0 = xTaskGetHandle("CORE0");    //retrieve a task handle for the first core
    const BaseType_t c0NtfStatus = xTaskNotify(core0, 1, eSetValueWithOverwrite);    //notify the first core that it can start running the web server
    Log.info(F("Main Core 1 Setup completed, CORE0 notified of WebServer %d. System status: %#hX"), c0NtfStatus, sysInfo->getSysStatus());
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
