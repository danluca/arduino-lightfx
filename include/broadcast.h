// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_BROADCAST_H
#define ARDUINO_LIGHTFX_BROADCAST_H

#include "Arduino.h"
#include "Event.h"
#include "config.h"
#include "fixed_queue.h"
#include "log.h"

extern FixedQueue<IPAddress*, 10> fxBroadcastRecipients;
extern events::Event<void(void)> evBroadcast;

void broadcastSetup();
void fxBroadcast();


#endif //ARDUINO_LIGHTFX_BROADCAST_H
