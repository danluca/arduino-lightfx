//
// Copyright by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_LOG_H
#define LIGHTFX_LOG_H

#ifndef LOGGING_ENABLED
#define LOGGING_ENABLED     0
#endif

#include <PicoLog.h>

/**
 * Initializes the logging subsystem.
 * Inspects hardware watchdog reset reasons and scratch register markers to identify abnormal resets,
 * captures any pre-reboot crash log present in uninitialized RAM, and starts the active logger.
 */
void log_setup();

/**
 * Persists any crash log snapshot captured during boot to LittleFS at /logs/crash_NNNN.log.
 * Automatically rotates out oldest crash logs to retain at most 5 files.
 */
void saveRebootCrashLog();

#endif //LIGHTFX_LOG_H
