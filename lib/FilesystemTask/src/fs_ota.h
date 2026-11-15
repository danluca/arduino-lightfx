#pragma once

#include <LittleFS.h>
#include <PicoOTA.h>

// Only call from the task that owns LittleFS. commit() closes the command file.
inline bool stageOtaCommand(FS &filesystem, PicoOTA &ota, const char *path) {
    if (&filesystem != &LittleFS)
        return false;
    if (filesystem.exists(_OTA_COMMAND_FILE) && !filesystem.remove(_OTA_COMMAND_FILE))
        return false;
    ota.begin();
    if (ota.addFile(path) && ota.commit())
        return true;
    // A short write must not leave a command file for the bootloader to consume.
    filesystem.remove(_OTA_COMMAND_FILE);
    return false;
}
