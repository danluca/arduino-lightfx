// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_BROADCAST_H
#define ARDUINO_LIGHTFX_BROADCAST_H

#include "log.h"

enum BroadcastState:uint8_t {Uninitialized, Configured, Broadcasting, Waiting};

void commSetup();
void commRun();
void postFxChangeEvent(uint16_t index);
void postTimeSetupCheck();

#endif //ARDUINO_LIGHTFX_BROADCAST_H
