// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef LOGPROXY_H
#define LOGPROXY_H

#if LOGGING_ENABLED == 1
#include "PicoLog.h"
#define log_debug Log.debug
#define log_info Log.info
#define log_warn Log.warn
#define log_error Log.error
#else
#define log_debug(...)
#define log_info(...)
#define log_warn(...)
#define log_error(...)
#endif
#endif //LOGPROXY_H
