// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_BROADCAST_H
#define ARDUINO_LIGHTFX_BROADCAST_H

enum BroadcastState:uint8_t {Uninitialized, Configured, Broadcasting, Waiting};

void commSetup();
void commRun();
void postFxChangeEvent(uint16_t index);
void postTimeSetupCheck();
void enqueueAlarmSetup();

#endif //ARDUINO_LIGHTFX_BROADCAST_H
