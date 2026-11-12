//
// Copyright by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_LOG_H
#define LIGHTFX_LOG_H

#ifndef LOGGING_ENABLED
#define LOGGING_ENABLED     0
#endif

#include <PicoLog.h>

void log_setup();
void saveRebootCrashLog();

#endif //LIGHTFX_LOG_H
