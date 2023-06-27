//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_LOG_H
#define LIGHTFX_LOG_H

// *************************************************************************
//  Uncomment line below to fully disable logging, and reduce project size
// ************************************************************************
#define DISABLE_LOGGING

#include <ArduinoLog.h>

void log_setup();
void logPrefix(Print* _logOutput, int logLevel);

#endif //LIGHTFX_LOG_H
