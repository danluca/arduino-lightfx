// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//
#include <Arduino.h>

#include "config.h"
#include "constants.hpp"
#include "hardware/watchdog.h"
#include "log.h"
#include "sysinfo_internal.h"
#include "version.h"

namespace {

constexpr auto unknown = "N/A";
constexpr auto sysInfoFmt = "SYSTEM INFO\n  CPU ROM %d [%.1f MHz] CORE %d\n  FreeRTOS version %s\n  Arduino PICO version %s [SDK %s]\n  Board UID 0x%s name '%s'\n  MAC Address %s\n  Device name %s build version %s at %s\n  Flash size %u";

#if LOGGING_ENABLED == 1
const char *resetReasonToString(const RP2040::resetReason_t reason) {
    switch (reason) {
        case RP2040::UNKNOWN_RESET: return unknown;
        case RP2040::PWRON_RESET: return csPowerOn;
        case RP2040::RUN_PIN_RESET: return csPinReset;
        case RP2040::SOFT_RESET: return csSoftReset;
        case RP2040::WDT_RESET: return csWatchdog;
        case RP2040::DEBUG_RESET: return csDebug;
        case RP2040::GLITCH_RESET: return csGlitch;
        case RP2040::BROWNOUT_RESET: return csBrownout;
        default: return unknown;
    }
}

const char *resetMarkerToString(const uint32_t marker) {
    switch (marker) {
        case kResetMarkerNone: return "none";
        case kResetMarkerPanic: return "panic";
        case kResetMarkerAssert: return "assert";
        case kResetMarkerHardFault: return "hardfault";
        case kResetMarkerMalloc: return "malloc_failed";
        case kResetMarkerStackOverflow: return "stack_overflow";
        case kResetMarkerFxStall: return "fx_stall";
        case kResetMarkerOta: return "ota";
        case kResetMarkerReboot: return "reboot";
        case kResetMarkerUnknown: return "unknown";
        default: return "other";
    }
}

const char *fxStageToString(const uint32_t stage) {
    switch (stage) {
        case kFxStageNone: return "none";
        case kFxStageEnter: return "enter";
        case kFxStageAfterQueue: return "after_queue";
        case kFxStageAfterOtaCheck: return "after_ota_check";
        case kFxStageFirmwareUpgrade: return "fw_upgrade";
        case kFxStageBeforeLoop: return "before_loop";
        case kFxStageAfterLoop: return "after_loop";
        case kFxStageAfterPing: return "after_ping";
        default: return "unknown";
    }
}

const char *fsBlockedActionToString(const uint8_t action) {
    switch (action) {
        case 0: return "READ_FILE";
        case 1: return "WRITE_FILE";
        case 2: return "WRITE_FILE_ASYNC";
        case 3: return "APPEND_FILE";
        case 4: return "APPEND_FILE_BIN";
        case 5: return "RENAME";
        case 6: return "DELETE";
        case 7: return "EXISTS";
        case 8: return "FORMAT";
        case 9: return "LIST_FILES";
        case 10: return "INFO";
        case 11: return "STAT";
        case 12: return "MAKE_DIR";
        case 13: return "SHA256";
        default: return "UNKNOWN";
    }
}
#endif

} // namespace

/**
 * Logs detailed system information for debugging and diagnostic purposes.
 * Called from CORE0 task.
 * This function outputs various system-level details, including:
 * - CPU ROM version and core speed
 * - FreeRTOS, Arduino PICO, and SDK version details
 * - Board identifier, board name, and MAC address
 * - Device name and flash memory size
 * - System reset reason with detailed status codes
 */
void logSystemInfo() {
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;
    log_info(sysInfoFmt, rp2040_rom_version(), RP2040::f_cpu() / 1000000.0, RP2040::cpuid(), tskKERNEL_VERSION_NUMBER, ARDUINO_PICO_VERSION_STR, PICO_SDK_VERSION_STRING,
               sysInfo->getBoardId().c_str(), BOARD_NAME, sysInfo->getMacAddress().c_str(), DEVICE_NAME, sysInfo->getBuildVersion().c_str(), sysInfo->getBuildTime().c_str(),
               sysInfo->get_flash_capacity());
    log_info(F("System reset reason %s"), resetReasonToString(rp2040.getResetReason()));
    const uint32_t resetMarker = watchdog_hw->scratch[kResetMarkerScratchIndex];
    log_info(F("System reset marker %s (0x%08lX)"), resetMarkerToString(resetMarker), resetMarker);
    watchdog_hw->scratch[kResetMarkerScratchIndex] = kResetMarkerNone;
    const uint32_t fxHeartbeat = watchdog_hw->scratch[kFxHeartbeatScratchIndex];
    log_info(F("FX heartbeat marker 0x%08lX"), fxHeartbeat);
    watchdog_hw->scratch[kFxHeartbeatScratchIndex] = 0u;
    const uint32_t fxStage = watchdog_hw->scratch[kFxStageScratchIndex];
    log_info(F("FX stage marker %s (0x%08lX)"), fxStageToString(fxStage), fxStage);
    watchdog_hw->scratch[kFxStageScratchIndex] = kFxStageNone;
#if DIAG_CORE_HEARTBEATS
    const uint32_t core0Heartbeat = watchdog_hw->scratch[kCore0HeartbeatScratchIndex];
    log_info(F("CORE0 heartbeat marker 0x%08lX"), core0Heartbeat);
    watchdog_hw->scratch[kCore0HeartbeatScratchIndex] = 0u;
    const uint32_t core1Heartbeat = watchdog_hw->scratch[kCore1HeartbeatScratchIndex];
    log_info(F("CORE1 heartbeat marker 0x%08lX"), core1Heartbeat);
    watchdog_hw->scratch[kCore1HeartbeatScratchIndex] = 0u;
#endif
    const uint32_t fsBlocked = watchdog_hw->scratch[kFsBlockedScratchIndex];
    if ((fsBlocked & 0xFF000000u) == kFsBlockedMagic) {
        const uint8_t op = static_cast<uint8_t>((fsBlocked >> 16) & 0xFFu);
        const uint16_t waitedSeconds = static_cast<uint16_t>(fsBlocked & 0xFFFFu);
        log_info(F("FS blocked marker op=%s (%u) waited=%u sec"), fsBlockedActionToString(op), op, waitedSeconds);
    } else if (fsBlocked != 0u) {
        log_info(F("FS blocked marker raw 0x%08lX"), fsBlocked);
    }
    watchdog_hw->scratch[kFsBlockedScratchIndex] = 0u;

    extern char __exidx_start;
    extern char __exidx_end;
    extern char __etext;
    extern char __data_start__;
    extern char __preinit_array_start;
    extern char __preinit_array_end;
    extern char __init_array_start;
    extern char __init_array_end;
    extern char __fini_array_start;
    extern char __fini_array_end;
    extern char __data_end__;
    extern char __bss_start__;
    extern char __bss_end__;
    extern char __end__;
    extern char __HeapLimit;
    extern char __StackLimit;
    extern char __StackTop;
    extern char __StackBottom;
    extern char __StackOneTop;
    extern char __StackOneBottom;
    extern uint32_t __scratch_x_source__;
    extern uint32_t __scratch_y_source__;
    log_info(F("Memory map pointers:"));
    log_info(F("  .text end:            __etext       = %#X"), (uint32_t)&__etext);
    log_info(F("  .data start/end:      __data_start__/__data_end__ = %#X/%#X"), (uint32_t)&__data_start__, (uint32_t)&__data_end__);
    log_info(F("  .bss start/end:       __bss_start__/__bss_end__   = %#X/%#X"), (uint32_t)&__bss_start__, (uint32_t)&__bss_end__);
    log_info(F("  .exidx start/end:     __exidx_start__/__exidx_end__ = %#X/%#X"), (uint32_t)&__exidx_start, (uint32_t)&__exidx_end);
    log_info(F("  .preinit_array start/end: __preinit_array_start__/__preinit_array_end__ = %#X/%#X"), (uint32_t)&__preinit_array_start, (uint32_t)&__preinit_array_end);
    log_info(F("  .init_array start/end:    __init_array_start__/__init_array_end__     = %#X/%#X"), (uint32_t)&__init_array_start, (uint32_t)&__init_array_end);
    log_info(F("  .fini_array start/end:    __fini_array_start__/__fini_array_end__     = %#X/%#X"), (uint32_t)&__fini_array_start, (uint32_t)&__fini_array_end);
    log_info(F("  Program end markers:  __end__       = %#X"), (uint32_t)&__end__);
    log_info(F("  Heap limits:          __HeapLimit   = %#X"), (uint32_t)&__HeapLimit);
    log_info(F("  Stack limits CORE0:         __StackLimit  = %#X; __StackTop = %#X; __StackBottom = %#X"), (uint32_t)&__StackLimit, (uint32_t)&__StackTop, (uint32_t)&__StackBottom);
    log_info(F("  Stack limits CORE1:         __StackLimit  = %#X; __StackTop = %#X; __StackBottom = %#X"), (uint32_t)&__StackLimit, (uint32_t)&__StackOneTop, (uint32_t)&__StackOneBottom);
    log_info(F("  Scratch RAM start:    __scratch_x_start__ = %#X; __scratch_y_start__ = %#X"), __scratch_x_source__, __scratch_y_source__);
#endif
}

/**
 * Logs the current system state, including system status and formatted uptime.
 */
void logSystemState() {
#if LOGGING_ENABLED == 1
    if (!Log.isEnabled(INFO))
        return;
    char buf[20];
    const unsigned long uptime = millis();
    snprintf(buf, 16, "%3luD %2luH %2lum", uptime / 86400000l, (uptime / 3600000l % 24), (uptime / 60000 % 60));
    log_info(F("System state: %#hX; uptime %s"), sysInfo->getSysStatus(), buf);
#endif
}
