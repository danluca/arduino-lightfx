// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//
#include <Arduino.h>

#include "constants.hpp"
#include "sysinfo.h"

namespace {

constexpr auto unknown = "N/A";

} // namespace

unsigned long prevStatTime = 0;
unsigned long prevIdleTime = 0;
SysInfo *sysInfo = nullptr;

const char *taskStatusToString(const eTaskState state) {
    switch (state) {
        case eReady: return csReady;
        case eBlocked: return csBlocked;
        case eSuspended: return csSuspended;
        case eDeleted: return csDeleted;
        case eRunning: return csRunning;
        case eInvalid: return csInvalid;
        default: return unknown;
    }
}

void SysInfo::begin() {
}
