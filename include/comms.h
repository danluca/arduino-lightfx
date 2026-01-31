// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_BROADCAST_H
#define ARDUINO_LIGHTFX_BROADCAST_H

#include <Arduino.h>
#include <vector>
#include <functional>

enum BroadcastState:uint8_t {Uninitialized, Configured, Broadcasting, Waiting};

void commSetup();
void commRun();
void postFxChangeEvent(uint16_t index);
void postTimeSetupCheck();
void enqueueAlarmSetup();

/**
 * Iterates over active client IP addresses and invokes the provided consumer function for each.
 * @param consumer function to receive each active IPAddress
 */
void forEachActiveClientIP(const std::function<void(const arduino::IPAddress&)>& consumer);

/**
 * Iterates over known client IP addresses and invokes the provided consumer function for each.
 * @param consumer function to receive each known IPAddress
 */
void forEachKnownClientIP(const std::function<void(const arduino::IPAddress&)>& consumer);

#endif //ARDUINO_LIGHTFX_BROADCAST_H
