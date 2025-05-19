//
// Copyright 2023,2024,2025 by Dan Luca. All rights reserved
//
#pragma once
#ifndef LIGHTFX_EFX_SETUP_H
#define LIGHTFX_EFX_SETUP_H

#include <FastLED.h>
#include "LedEffect.h"
#include "EffectRegistry.h"
#include "config.h"
#include "global.h"
#include "PaletteFactory.h"
#include "constants.hpp"
#include "fxutil.h"
#include "log.h"
#if LOGGING_ENABLED == 1
#include "stringutils.h"
#endif

using namespace fx;

typedef void (*setupFunc)();

extern CRGB leds[NUM_PIXELS];
extern CRGBArray<PIXEL_BUFFER_SPACE> frame;
extern CRGBSet tpl;
extern CRGBSet others;
extern CRGBSet ledSet;
extern uint16_t stripShuffleIndex[NUM_PIXELS];
extern CRGBPalette16 palette;
extern CRGBPalette16 targetPalette;
extern OpMode mode;
extern uint8_t brightness;
extern uint8_t stripBrightness;
extern bool stripBrightnessLocked;
extern uint8_t colorIndex;
extern uint8_t lastColorIndex;
extern uint8_t fade;
extern uint8_t hue;
extern uint8_t dotBpm;
extern uint8_t saturation;
extern uint8_t delta;
extern uint16_t hueDiff;
extern bool dirFwd;
extern int32_t dist;
extern uint16_t totalAudioBumps;
extern volatile uint16_t audioBumpThreshold;
extern volatile uint16_t maxAudio[AUDIO_HIST_BINS_COUNT];
extern volatile bool fxBump;
extern volatile bool fxBroadcastEnabled;
extern volatile uint16_t speed;
extern volatile uint16_t curPos;

void ledStripInit();
void resetGlobals();
void saveFxState();
void readFxState();
void fx_setup();
void fx_run();

namespace FxA {
    void fxRegister();
}
namespace FxB {
    void fxRegister();
}
namespace FxC {
    void fxRegister();
}
namespace FxD {
    void fxRegister();
}
namespace FxE {
    void fxRegister();
}
namespace FxF {
    void fxRegister();
}
namespace FxH {
    void fxRegister();
}
namespace FxI {
    void fxRegister();
}
namespace FxJ {
    void fxRegister();
}
namespace FxK {
    void fxRegister();
}

extern const setupFunc categorySetup[];


#endif //LIGHTFX_EFX_SETUP_H
