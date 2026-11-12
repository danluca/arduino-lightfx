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

#include "logPico.h"

#if LOGGING_ENABLED == 1
/** Emits a debug log message when logging is enabled. */
#define log_debug Log.debug
/** Emits an informational log message. */
#define log_info Log.info
/** Emits a warning log message. */
#define log_warn Log.warn
/** Emits an error log message. */
#define log_error Log.error
/** Writes raw data directly to log. */
#define log_write Log.write
#else
/** Debug logging is compiled out when logging is disabled. */
#define log_debug(...)
/** Emits an informational log message to circular RAM buffer. */
#define log_info Log.info
/** Emits a warning log message to circular RAM buffer. */
#define log_warn Log.warn
/** Emits an error log message to circular RAM buffer. */
#define log_error Log.error
/** Writes raw data directly to circular RAM buffer. */
#define log_write Log.write
#endif

#endif
