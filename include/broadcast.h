// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_BROADCAST_H
#define ARDUINO_LIGHTFX_BROADCAST_H

#include "config.h"
#include "fixed_queue.h"
#include "log.h"

enum BroadcastState:uint8_t {Uninitialized, Configured, Broadcasting, Waiting};

void postFxChangeEvent(uint16_t index);
void postWiFiSetupEvent();

#endif //ARDUINO_LIGHTFX_BROADCAST_H
