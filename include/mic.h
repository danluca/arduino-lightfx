//
// Copyright 2023,2024,2025,2026 by Dan Luca. All rights reserved
//

#ifndef ARDUINO_LIGHTFX_MIC_H
#define ARDUINO_LIGHTFX_MIC_H

#include "Arduino.h"

extern mutex_t audioStatsMutex;

void mic_setup();

void mic_run();

#endif //ARDUINO_LIGHTFX_MIC_H
