// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_SYSINFO_H
#define ARDUINO_LIGHTFX_SYSINFO_H

#include "log.h"
#include <flash.h>
#include "fixed_queue.h"
#ifndef DISABLE_LOGGING
#include "config.h"
#include <rtx_lib.h>
#endif

#define MAX_WATCHDOG_REBOOT_TIMESTAMPS  10      // max number of watchdog reboots to keep in the list
typedef FixedQueue<time_t, MAX_WATCHDOG_REBOOT_TIMESTAMPS> WatchdogQueue;

extern char boardId[];
extern unsigned long prevStatTime;
extern us_timestamp_t prevIdleTime;
extern WatchdogQueue wdReboots;

void logThreadInfo(osThreadId threadId);
void logAllThreadInfo();
void logHeapAndStackInfo();
void logSystemInfo();
void logCPUStats();
const char* fillBoardId();
inline void readSysInfo() { };
inline void saveSysInfo() { };


#endif //ARDUINO_LIGHTFX_SYSINFO_H
