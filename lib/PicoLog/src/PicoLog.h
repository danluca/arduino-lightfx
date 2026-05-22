// Copyright (c) by Dan Luca. All rights reserved.
//
#pragma once
#ifndef PICOLOG_H
#define PICOLOG_H

#ifndef LOGGING_ENABLED
#define LOGGING_ENABLED 0
#endif
#ifndef LOG_BYPASS_BUFFER
#define LOG_BYPASS_BUFFER false
#endif

#if LOGGING_ENABLED == 1
#include "logPico.h"
#define log_debug Log.debug
#define log_info Log.info
#define log_warn Log.warn
#define log_error Log.error
#define log_write Log.write
#else
#define log_debug(...)
// #define log_info(...) busy_wait_us(50)
// #define log_warn(...) busy_wait_us(50)
// #define log_error(...) busy_wait_us(50)
#define log_info(...)
#define log_warn(...)
#define log_error(...)
#define log_write(...)
#endif

#endif
