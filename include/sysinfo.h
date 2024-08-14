// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_SYSINFO_H
#define ARDUINO_LIGHTFX_SYSINFO_H

#include "log.h"
#include <flash.h>
#ifndef DISABLE_LOGGING
#include <rtx_lib.h>
#endif

extern char boardId[];
void logThreadInfo(osThreadId threadId);
void logAllThreadInfo();
void logHeapAndStackInfo();
void logSystemInfo();
const char* fillBoardId();

#endif //ARDUINO_LIGHTFX_SYSINFO_H
