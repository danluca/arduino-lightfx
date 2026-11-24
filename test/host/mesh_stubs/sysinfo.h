// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//

#pragma once
#include "Arduino.h"

enum class SysStatus { Wifi };
struct SystemInfo {
    bool wifi = true;
    String id = "master-2350";
    String name = "Tree";
    bool isSysStatus(SysStatus) const { return wifi; }
    const String &getBoardId() const { return id; }
    const String &getDeviceName() const { return name; }
};
inline SystemInfo testSystemInfo;
inline auto sysInfo = &testSystemInfo;
