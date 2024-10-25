// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_BROADCAST_H
#define ARDUINO_LIGHTFX_BROADCAST_H

#include "config.h"
#include "fixed_queue.h"
#include "log.h"

#if BROADCAST_MASTER
void broadcastSetup();
void fxBroadcast();

#endif

void postFxChangeEvent();

#endif //ARDUINO_LIGHTFX_BROADCAST_H
