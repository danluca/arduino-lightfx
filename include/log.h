//
// Copyright 2023,2024 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_LOG_H
#define LIGHTFX_LOG_H

// *************************************************************************
//  Uncomment line below to fully disable logging, and reduce project size
// ************************************************************************
#define DISABLE_LOGGING

#include <ArduinoLog.h>
#include <TimeLib.h>

extern time_t logTimeOffset;

void log_setup();
void logPrefix(Print* _logOutput, int logLevel);
void logExtraFormats(Print* _logOutput, char fmt, va_list *args);

#endif //LIGHTFX_LOG_H
