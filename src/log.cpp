//
// Copyright (c) by Dan Luca. All rights reserved
//
#include "log.h"
#include <FastLED.h>
#include "util.h"
#include "constants.hpp"
#include "filesystem.h"
#include "sysinfo.h"
#include "hardware/watchdog.h"
#include <memory>
#include <vector>
#include <algorithm>

static std::unique_ptr<String> g_crashLogSnapshot;

void log_setup() {
    Log.init();

    const RP2040::resetReason_t resetReason = rp2040.getResetReason();
    const uint32_t resetMarker = watchdog_hw->scratch[kResetMarkerScratchIndex];
    const bool isAbnormal = (resetReason == RP2040::WDT_RESET) ||
        (resetMarker != kResetMarkerNone && resetMarker != kResetMarkerReboot && resetMarker != kResetMarkerOta);

    if (isAbnormal && Log.hasValidCrashLog()) {
        g_crashLogSnapshot = std::make_unique<String>();
        g_crashLogSnapshot->reserve(Log.getBufferSize() + 384);

        const uint32_t fxStage = watchdog_hw->scratch[kFxStageScratchIndex];
        const uint32_t fsBlocked = watchdog_hw->scratch[kFsBlockedScratchIndex];
        char hdr[256];
        snprintf(hdr, sizeof(hdr),
            "=== CRASH LOG ===\n"
            "Reset Reason: %s\n"
            "Reset Marker: %s (0x%08lX)\n"
            "FX Stage: %s (0x%08lX)\n"
            "FS Blocked: 0x%08lX\n"
            "Log Buffer Size: %zu bytes\n"
            "=== LOG BUFFER START ===\n",
            resetReasonToString(resetReason),
            resetMarkerToString(resetMarker), static_cast<unsigned long>(resetMarker),
            fxStageToString(fxStage), static_cast<unsigned long>(fxStage),
            static_cast<unsigned long>(fsBlocked),
            Log.getBufferSize());
        g_crashLogSnapshot->concat(hdr);
        Log.snapshotCrashLog(*g_crashLogSnapshot);
        g_crashLogSnapshot->concat("\n=== LOG BUFFER END ===\n");
    } else {
        Log.resetBuffer();
    }

#if LOGGING_ENABLED == 1
    if (!Serial) {
        Serial.begin(115200); // initialize serial communication - note the rate parameter is likely ignored, HW negotiates with the host PC the proper rate
        while (!Serial) {
            taskDelay(100);     //wait for the serial connection to be initiated by the PC
        }
        Serial.println(F("================================================================================"));
    }
    Log.begin(&Serial, INFO);
    taskDelay(500);
    Log.info(F("Logging system setup OK"));
#else
    Log.begin(nullptr, INFO);
    Log.info(F("Logging system setup OK (32KB RAM buffer)"));
#endif
}

void saveRebootCrashLog() {
    if (!g_crashLogSnapshot || g_crashLogSnapshot->length() == 0) {
        g_crashLogSnapshot.reset();
        return;
    }

    (void)SyncFsImpl.mkdir("/logs");

    std::deque<FileInfo> fileList;
    (void)SyncFsImpl.list("/logs", &fileList);

    struct CrashFile {
        String path;
        uint32_t index;
    };
    std::vector<CrashFile> crashFiles;
    uint32_t maxIndex = 0;

    for (const auto &fi : fileList) {
        if (fi.isDir) continue;
        if (fi.name.startsWith("crash_") && fi.name.endsWith(".log")) {
            const String numStr = fi.name.substring(6, fi.name.length() - 4);
            if (const auto idx = static_cast<uint32_t>(numStr.toInt()); idx > 0) {
                crashFiles.push_back({fi.path, idx});
                if (idx > maxIndex) {
                    maxIndex = idx;
                }
            }
        }
    }

    constexpr size_t MAX_CRASH_LOGS = 5;
    if (crashFiles.size() >= MAX_CRASH_LOGS) {
        std::sort(crashFiles.begin(), crashFiles.end(), [](const CrashFile &a, const CrashFile &b) {
            return a.index < b.index;
        });
        const size_t toDelete = crashFiles.size() - MAX_CRASH_LOGS + 1;
        for (size_t i = 0; i < toDelete; ++i) {
            (void)SyncFsImpl.remove(crashFiles[i].path.c_str());
            log_info(F("Rotated out old crash log: %s"), crashFiles[i].path.c_str());
        }
    }

    const uint32_t newIndex = maxIndex + 1;
    char newPath[32];
    snprintf(newPath, sizeof(newPath), "/logs/crash_%04lu.log", static_cast<unsigned long>(newIndex));

    const size_t written = SyncFsImpl.writeFile(newPath, g_crashLogSnapshot.get());
    log_info(F("Saved crash log [%zu bytes] to %s"), written, newPath);

    g_crashLogSnapshot.reset();
}

