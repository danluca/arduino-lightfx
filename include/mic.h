//
// Copyright 2023,2024,2025 by Dan Luca. All rights reserved
//

#ifndef ARDUINO_LIGHTFX_MIC_H
#define ARDUINO_LIGHTFX_MIC_H

#include "../lib/Utils/src/circular_buffer.h"

extern CircularBuffer<short> *audioData;

void mic_setup();

void mic_run();

void clearLevelHistory();

#endif //ARDUINO_LIGHTFX_MIC_H
