///////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2023 (c) Dan Luca
///////////////////////////////////////////////////////////////////////////////////////////////////
// Collection of light strip effects with the ability to be configured through Wi-Fi
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <LittleFS.h>
#include <SchedulerExt.h>
#include "filesystem.h"
#include "net_setup.h"
#include "efx_setup.h"
#include "sysinfo.h"
#include "diag.h"
#include "comms.h"
#include "FxSchedule.h"
#include "log.h"
#include "util.h"
#include "task_msg.h"
#include "web_server.h"
#include "ota_upgrade.h"
#include "HealthMonitor.h"
#include "board_config.h"
#include "hardware/watchdog.h"
#include "constants.hpp"

/**
 * TASK ALLOCATIONS
 * First Core
 *   - CORE0 (default task - setup, loop) - Web and communications, starts at configMAX_PRIORITIES / 2 and is lowered by one after setup
 *   - ALM - alarm processing and miscellaneous actions, configMAX_PRIORITIES / 2 - 1
 *   - FS - filesystem interaction, one level above its caller (normally configMAX_PRIORITIES / 2 + 1)
 *   - IdleCore0 - framework flash-coordination task, configMAX_PRIORITIES - 1
 *   - USB - framework task on core 0 at configMAX_PRIORITIES - 2 when enabled; disabled in release mode
 *
 * Second Core
 *   - CORE1 (default task - setup1, loop1) - diagnostics, configMAX_PRIORITIES / 2
 *   - FX - light effects, configMAX_PRIORITIES / 2 + 1
 *   - IdleCore1 - framework flash-coordination task, configMAX_PRIORITIES - 1
 *
 * Other relevant tasks:
 *   - SRL - serial logging on either core (affinity 0xFFFFFFFF), configMAX_PRIORITIES / 2 - 2
 *   - LWIP - framework task on core 0 at configMAX_PRIORITIES - 2
 *   - IDLE0, IDLE1, Tmr Svc - framework idle and timer-service tasks
 * TaskDef stack sizes are FreeRTOS stack depths in words; CORE0 and CORE1 use 3072 words each.
 */

static void alarm_misc_begin();
static void alarm_misc_run();
[[maybe_unused]] static void logTaskProbe();
// FX stays above CORE1; ALM matches CORE0 after CORE0's setup-time priority reduction.
constexpr TaskDef fxTasks {.setup = fx_setup, .loop = fx_run, .stackSize = 1536, .threadName = csFxTask, .priority = TaskPriorityPolicy::Fx, .core = CORE_1};
constexpr TaskDef alarmTasks {.setup = alarm_misc_begin, .loop = alarm_misc_run, .stackSize = 1536, .threadName = "ALM", .priority = TaskPriorityPolicy::Alarm, .core = CORE_0};
bool core1_separate_stack = true;

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
    static constexpr AlmAction msgSetup = ALARM_SETUP;
    if (const BaseType_t qResult = xQueueSend(almQueue, &msgSetup, 0); qResult != pdTRUE)
        log_error(F("Error sending ALARM_SETUP message to ALM queue - error %ld"), qResult);
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
 * Actions are specified using the `AlmAction` enum, and the function executes corresponding
 * behavior based on the received action type.
 *
 * Behavior:
 * - Executes the appropriate action based on the `AlmAction` received:
 *   - ALARM_SETUP: Calls `alarm_setup()` to initialize alarm settings.
 *   - ALARM_CHECK: Calls `alarm_check()` to verify current alarm status.
 *   - SAVE_SYS_INFO: Calls `saveSysInfo()` to persist system state information.
 *   - HOLIDAY_UPDATE: Calls `holidayUpdate()` to refresh the current holiday.
 * - Logs an error if an unsupported or unrecognized action is encountered.
 *
 * Notes:
 * - `almQueue` is used as the primary communication mechanism for this task.
 * - Relies on external functions, namely `alarm_setup`, `alarm_check`, `saveSysInfo` and `holidayUpdate` for specific actions.
 * - Executes on the ALM task, assigned to CORE_0 in the system configuration (see `alarmTasks`).
 */
void alarm_misc_run() {
    AlmAction action;
    // wait indefinitely for a message to be received
    if (pdFALSE == xQueueReceive(almQueue, &action, portMAX_DELAY)) {
        return;
    }
    switch (action) {
        case ALARM_SETUP: alarm_setup(); break;
        case ALARM_CHECK: alarm_check(); break;
        case SAVE_SYS_INFO: saveSysInfo(); break;
        case SAVE_SYS_INFO_DONE: log_info(F("SysInfo saved successfully")); break;
        case HOLIDAY_UPDATE: holidayUpdate(); break;
        default:
            log_error(F("Misc Action %hu not supported"), action);
    }
}

static void filesystem_setup() {
    SyncFsImpl.begin(LittleFS);
    log_info(F("Filesystem setup completed"));
    sysInfo->setSysStatus(SysStatus::Filesystem);
}

static void logTaskProbeForHandle(const char *label, const TaskHandle_t handle) {
    if (handle == nullptr) {
        log_warn(F("Task probe %s: handle not found"), label);
        return;
    }

    TaskStatus_t status {};
    vTaskGetInfo(handle, &status, pdTRUE, eInvalid);
    log_info(F("Task probe %s: state=%s(%d) prio=%lu/%lu stackHwm=%u taskNum=%u coreMask=0x%02lx"),
        label,
        taskStatusToString(status.eCurrentState),
        static_cast<int>(status.eCurrentState),
        status.uxCurrentPriority,
        status.uxBasePriority,
        static_cast<unsigned>(status.usStackHighWaterMark),
        static_cast<unsigned>(status.xTaskNumber),
        static_cast<unsigned long>(status.uxCoreAffinityMask));
}

static void logTaskProbe() {
    static uint32_t lastLogMs = 0;
    const uint32_t nowMs = millis();
    if (nowMs - lastLogMs < 1000u)
        return;
    lastLogMs = nowMs;

    logTaskProbeForHandle(csFxTask, xTaskGetHandle(csFxTask));
    logTaskProbeForHandle(csCORE1, xTaskGetHandle(csCORE1));
}

//===First core tasks===
/**
 * Core 0 Setup LED strip and global data structures
 * CORE0's stack depth is configured as 3072 words (12 KiB) by configCORE0_TASK_STACK_DEPTH.
 */
void setup() {
    taskDelay(2000);    //safety delay
    SysInfo::setupStateLED();
    log_setup();

    // RP2040::enableDoubleResetBootloader();   //that's just a good idea overall

    const String boardUid = rp2040.getChipID();
    const bool knownBoard = selectBoardConfiguration(boardUid.c_str());
    const BoardConfiguration &configuration = activeBoardConfiguration();
    if (knownBoard)
        log_info(F("Known board UID %s selected profile %s: %hu pixels, frame %hu, %s IP"), boardUid.c_str(), configuration.deviceName,
                 configuration.numPixels, configuration.frameSize, configuration.useDhcp() ? "DHCP" : "static");
    else
        log_warn(F("Unrecognized board UID %s; using safe defaults (DHCP, %hu pixels, brightness %hu)"), boardUid.c_str(),
                 configuration.numPixels, configuration.brightness);

    sysInfo = new SysInfo();    //system information object built once per run
    sysInfo->fillBoardId();
    filesystem_setup();
    sysInfo->begin();

    task_msg_setup();

    Scheduler.startTask(&alarmTasks);

    readSysInfo();

    const TaskHandle_t core1 = xTaskGetHandle(csCORE1);    //retrieve a task handle for the second core
    [[maybe_unused]] const BaseType_t c1Fx = xTaskNotify(core1, 1, eSetValueWithOverwrite);    //notify the second core that it can start running FX
    log_info(F("Basic components ok - CORE1 notified of starting FX %d. System status: %#hX"), c1Fx, sysInfo->getSysStatus());

    wifi_setup();           // blocking until we get WiFi
    taskDelay(2500);    // let the WiFi settle
    timeSetup(NTP_BOOT_WAIT_MS);    // starts NTP (lwIP SNTP client, sets the system clock) and waits for the first sync; the retry timer follows up if it does not come in time
    commSetup();
    web::server_setup();

    // notifies Core1 to start processing tasks that need WiFi
    [[maybe_unused]] const BaseType_t c1NtfStatus = xTaskNotify(core1, 2, eSetValueWithOverwrite);

    watchdogSetup();
    HealthMonitor::init();

    vTaskPrioritySet(nullptr, uxTaskPriorityGet(nullptr)-1);    //lower the priority of the main task to allow for other tasks to run
    taskDelay(250);         // leave reasonable time to the alarm task to set up
    //enqueues the alarm setup event if time is ok
    if (sysInfo->isSysStatus(SysStatus::Ntp))
        enqueueAlarmSetup();
    else
        log_warn(F("System time not yet synchronized with NTP, skipping alarm setup; retrying later"));

    //wait for the other core to finish all initializations before allowing web server to respond to requests
    // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    sysInfo->setSysStatus(SysStatus::Setup0);
    log_info(F("Main CORE0 Setup completed, CORE1 notified of WiFi %d. System status: %#hX"), c1NtfStatus, sysInfo->getSysStatus());
    saveRebootHealthEvent();    // must precede logSystemInfo() which clears the watchdog scratch registers
    logSystemInfo();
}

/**
 * Core 0 Main loop - runs the web actions
 */
void loop() {
    HealthMonitor::checkIn(HEALTH_CORE0);
    // logTaskProbe();
    web::webserver();
    // During FW upgrade, skip comms to prevent blocking the web server which handles the upload
    if (!HealthMonitor::isInOtaMode()) {
        commRun();
    }
    handle_fw_upgrade();
    vTaskDelay(10);   //this is important to allow other tasks to execute on core 0

    // static uint32_t lastCore0WdtPingMs = 0;
    // const uint32_t nowMsPing = millis();
    // if (nowMsPing - lastCore0WdtPingMs >= 1000u) {
        // lastCore0WdtPingMs = nowMsPing;
        //HealthMonitor::update(7000, 3000);
    // }
}


//===Second core tasks===
/**
 * Core 1 Setup communication and diagnostic tasks
 * CORE1's stack depth is configured as 3072 words (12 KiB) by configCORE1_TASK_STACK_DEPTH.
 * CORE1 runs at the framework midpoint; FX is one level higher and yields for one tick after each loop iteration.
 */
void setup1() {
    //wait for the main core to notify us that the core components are ready (filesystem, logging, secure element), not interested in the notification value
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    Scheduler.startTask(&fxTasks);
    taskDelay(250);         // leave reasonable time to FX task to set-up

    //wait for the main core to notify us that WiFi is ready, not interested in the notification value
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    diagSetup();

    // const TaskHandle_t core0 = xTaskGetHandle(csCORE0);    //retrieve a task handle for the first core
    // const BaseType_t c0NtfStatus = xTaskNotify(core0, 1, eSetValueWithOverwrite);    //notify the first core that it can start running the web server
    sysInfo->setSysStatus(SysStatus::Setup1);
    log_info(F("Main CORE1 Setup completed. System status: %#hX"), sysInfo->getSysStatus());
}

/**
 * Core 1 Main loop - runs the communication tasks
 */
void loop1() {
    HealthMonitor::checkIn(HEALTH_CORE1);
    diagExecute();
    vTaskDelay(7);
}

/**
 * Log an event to the console when stack overflow is encountered
 * @param xTask task handle for task that exceeded stack
 * @param pcTaskName name of the task that exceeded stack
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    watchdog_hw->scratch[kResetMarkerScratchIndex] = kResetMarkerStackOverflow;
#ifndef PIO_FRAMEWORK_ARDUINO_NO_USB
    if (Serial)
        Serial.printf("Stack overflow in task %s [%p]\n", pcTaskName, xTask);
#endif
    watchdog_reboot(0, 0, 10);
    while (true)
        tight_loop_contents();
}

/**
 * Log an event to the console when malloc fails
 */
void vApplicationMallocFailedHook() {
    watchdog_hw->scratch[kResetMarkerScratchIndex] = kResetMarkerMalloc;
#ifndef PIO_FRAMEWORK_ARDUINO_NO_USB
    if (Serial)
        Serial.println("pvPortMalloc failed");
#endif
    watchdog_reboot(0, 0, 10);
    while (true)
        tight_loop_contents();
}
