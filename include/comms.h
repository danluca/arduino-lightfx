// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_BROADCAST_H
#define ARDUINO_LIGHTFX_BROADCAST_H

#include <Arduino.h>
#include <vector>

enum BroadcastState:uint8_t {Uninitialized, Configured, Broadcasting, Waiting};

void commSetup();
void commRun();
void postFxChangeEvent(uint16_t index);
void postTimeSetupCheck();
void enqueueAlarmSetup();

/**
 * Get the list of active board IP addresses this board has identified (and may be controlling).
 * @return list of active board IP addresses
 */
std::vector<arduino::IPAddress> getActiveClientIPs();

#endif //ARDUINO_LIGHTFX_BROADCAST_H
