// Copyright (c) by Dan Luca. All rights reserved.
//

#include "ota_upgrade.h"
#include <SchedulerExt.h>
#include <LittleFS.h>
#include <PicoLog.h>
#include <PicoOTA.h>
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "constants.hpp"
#include "util.h"
#include "log.h"

bool upgrade_check();
void fw_upgrade();
void notifyFx();

/**
 * @brief Handles the firmware upgrade process.
 *
 * This method checks whether a firmware upgrade is available and if so, it initiates the
 * firmware upgrade process. A system reboot is triggered after the upgrade.
 *
 * This function is called periodically by CORE0 task to ensure the system
 * upgrades seamlessly when a new firmware version is uploaded.
 */
void handle_fw_upgrade() {
    if (upgrade_check()) {
        log_info(F("=====================================UPGRADE===================================="));
        notifyFx();     //we're starting an upgrade - set the lights on the upgrade mode
        log_info(F("Firmware upgrade available, stopping tasks, staging OTA command, and rebooting. See ya on the other side!"));
        fw_upgrade();
    }
}

/**
 * Check if this task - CORE0 - has been notified that a FW image has been successfully uploaded and verified.
 * Note: this method does not check whether the FW image file exists
 * @return true if we've been notified a firmware image has been uploaded successfully
 */
bool upgrade_check() {
    return ulTaskNotifyTake(pdTRUE, 0) == OTA_UPGRADE_NOTIFY;
}

/**
 * Notify the FX task that we're starting firmware upgrade
 */
void notifyFx() {
    if (const TaskHandle_t fxHandle = xTaskGetHandle(csFxTask)) {
        [[maybe_unused]] const BaseType_t fwNotif = xTaskNotify(fxHandle, OTA_UPGRADE_NOTIFY, eSetValueWithOverwrite);
        log_info(F("FX task has been notified of FW upgrade starting, notification status %d"), fwNotif);
    } else
        log_warn(F("FX task not found - unable to notify about FW upgrade"));
}

void fw_upgrade() {
    // Allow FX task time to display the firmware upgrade light pattern
    taskDelay(2000);

    // Keep watchdog active during staging to prevent unrecoverable hangs
    watchdog_update();
#ifndef PIO_FRAMEWORK_ARDUINO_NO_USB
    if (Serial)
        Serial.println(F(">> FWU:: Watchdog refreshed for OTA staging"));
#endif

    // Gracefully stop all application tasks managed by the scheduler so locks/mutexes are released
    Scheduler.stopAllTasks(false);
    taskDelay(250);
#ifndef PIO_FRAMEWORK_ARDUINO_NO_USB
    if (Serial)
        Serial.println(F(">> FWU:: All APP tasks stopped"));
#endif

    // Suspend CORE1 task to prevent concurrent flash execution or hardware access
    if (const TaskHandle_t core1Handle = xTaskGetHandle(csCORE1)) {
        vTaskSuspend(core1Handle);
#ifndef PIO_FRAMEWORK_ARDUINO_NO_USB
        if (Serial)
            Serial.println(F(">> FWU:: CORE1 suspended"));
#endif
    }

    watchdog_update();

    if (!LittleFS.exists(csFWImageFilename)) {
        log_error(F(">> FWU:: Firmware image %s not found on filesystem, aborting upgrade"), csFWImageFilename);
        reboot();
    }

    // Prepare the command file to flash the image
    picoOTA.begin();
    picoOTA.addFile(csFWImageFilename);
    picoOTA.commit();
    LittleFS.end();
#ifndef PIO_FRAMEWORK_ARDUINO_NO_USB
    if (Serial)
        Serial.println(F(">> FWU:: Firmware upgrade staged, triggering hardware reboot..."));
#endif

    // Set OTA reset marker and trigger immediate hardware watchdog reboot
    watchdog_hw->scratch[kResetMarkerScratchIndex] = kResetMarkerOta;
    watchdog_reboot(0, 0, 50);
    while (true)
        tight_loop_contents();
}
