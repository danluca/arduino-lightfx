// MIT License
//
// Copyright (c) 2026 by Dan Luca. All rights reserved.
//

#include <cstdarg>
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "constants.hpp"

namespace {
[[noreturn]] void reboot_with_marker(const uint32_t marker) {
    watchdog_hw->scratch[kResetMarkerScratchIndex] = marker;
    watchdog_reboot(0, 0, 10);
    while (true)
        tight_loop_contents();
}
}  // namespace

extern "C" {

// HardFault handler override (CMSIS-style).
void HardFault_Handler(void) {
    reboot_with_marker(kResetMarkerHardFault);
}

// Hard fault handler override (pico-sdk style).
void hard_fault_handler(void) {
    reboot_with_marker(kResetMarkerHardFault);
}

// Newlib assert hook.
void __assert_func(const char * /*file*/, int /*line*/, const char * /*func*/, const char * /*expr*/) {
    reboot_with_marker(kResetMarkerAssert);
}

// Pico SDK panic hook (weak to avoid collisions if provided by the core).
__attribute__((weak)) void panic(const char * /*fmt*/, ...) {
    reboot_with_marker(kResetMarkerPanic);
}

}  // extern "C"
