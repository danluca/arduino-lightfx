//
// Copyright 2023,2024 by Dan Luca. All rights reserved
//

#ifndef ARDUINO_LIGHTFX_MIC_H
#define ARDUINO_LIGHTFX_MIC_H

#include "PDM2040.h"

void mic_setup();

void mic_run();

void clearLevelHistory();

#endif //ARDUINO_LIGHTFX_MIC_H
