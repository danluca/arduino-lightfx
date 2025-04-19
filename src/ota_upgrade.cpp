// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "ota_upgrade.h"
#include <SchedulerExt.h>
#include <LittleFS.h>
#include <PicoLog.h>
#include <PicoOTA.h>
#include "constants.hpp"
#include "util.h"
#include "log.h"

bool upgrade_check();
void fw_upgrade();

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
        log_info(F("Firmware upgrade available, proceeding to disable watchdog, stop all tasks and write the command file. System will reboot and flash - see ya on the other side!"));
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

void fw_upgrade() {
    taskDelay(3000);
    //stop the watchdog
    watchdog_disable();
    if (Serial)
        Serial.println(F(">> FWU:: Watchdog disabled"));
    //stop all tasks
    Scheduler.suspendAllTasks();
    if (Serial)
        Serial.println(F(">> FWU:: All APP tasks suspended"));
    //prepare the command file to flash the image
    picoOTA.begin();
    picoOTA.addFile(csFWImageFilename);
    picoOTA.commit();
    LittleFS.end();
    if (Serial)
        Serial.println(F(">> FWU:: Firmware upgrade initiated, rebooting system to complete..."));
    //restart the system
    taskDelay(1000);
    rp2040.reboot();
}
