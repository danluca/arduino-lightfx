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

void handle_fw_upgrade() {
    if (upgrade_check()) {
        log_info(F("Firmware upgrade available, proceeding to stop all tasks and write the new image into flash. System will reboot - see ya on the other side!"));
        fw_upgrade();
    }
}

bool upgrade_check() {
    return ulTaskNotifyTake(pdTRUE, 0) == OTA_UPGRADE_NOTIFY;
}

void fw_upgrade() {
    taskDelay(3000);
    //stop all tasks
    Scheduler.stopAllTasks(true);
    taskDelay(1000);
    //stop the other core
    rp2040.idleOtherCore();
    //we're starting image upgrade
    taskDelay(1000);
    //flash the image
    picoOTA.begin();
    picoOTA.addFile(csFWImageFilename);
    picoOTA.commit();
    LittleFS.end();
    //restart the system
    taskDelay(2000);
    rp2040.reboot();
}
