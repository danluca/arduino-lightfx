// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_BROADCAST_H
#define ARDUINO_LIGHTFX_BROADCAST_H

#include "config.h"
#include "fixed_queue.h"
#include "log.h"

void broadcastSetup();
void fxBroadcast(const uint16_t index);

void postFxChangeEvent(const uint16_t index);

#endif //ARDUINO_LIGHTFX_BROADCAST_H
