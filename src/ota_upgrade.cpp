// Copyright (c) by Dan Luca. All rights reserved.
//

#include "ota_upgrade.h"
#include <atomic>
#include <Arduino.h>
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "constants.hpp"
#include "util.h"
#include "log.h"

namespace {
    std::atomic<bool> upgradePending{false};
}

void request_fw_upgrade() {
    // The filesystem task has already closed the image and committed the OTA command.
    upgradePending.store(true, std::memory_order_release);
}

bool firmware_upgrade_pending() {
    return upgradePending.load(std::memory_order_acquire);
}

void handle_fw_upgrade() {
    if (!firmware_upgrade_pending())
        return;

    log_info(F("Firmware upgrade staged; rebooting after the upgrade light pattern"));
    // Keep services and their locks alive. All staging ran through the FS task;
    // no direct LittleFS access, forced task deletion, or CORE1 suspension is needed.
    taskDelay(2000);
    watchdog_hw->scratch[kResetMarkerScratchIndex] = kResetMarkerOta;
    watchdog_reboot(0, 0, 50);
    while (true)
        tight_loop_contents();
}
