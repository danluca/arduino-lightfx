//
// Copyright 2023,2024,2025,2026 by Dan Luca. All rights reserved
//
#pragma once
#ifndef LIGHTFX_EFX_SETUP_H
#define LIGHTFX_EFX_SETUP_H

#include <FastLED.h>
#include <atomic>
#include "LedEffect.h"
#include "EffectRegistry.h"
#include "config.h"
#include "global.h"
#include "PaletteFactory.h"
#include "constants.hpp"
#include "fxutil.h"
#include "log.h"
#if LOGGING_ENABLED == 1
#include <stringutils.h>
#endif

using namespace fx;

typedef void (*setupFunc)();

extern volatile uint8_t brightness;
extern volatile uint8_t stripBrightness;
extern std::atomic<bool> stripBrightnessLocked;
extern volatile uint8_t colorIndex;
extern volatile uint8_t lastColorIndex;
extern volatile uint8_t fade;
extern volatile uint8_t hue;
extern volatile uint8_t dotBpm;
extern volatile uint8_t saturation;
extern volatile uint8_t delta;
extern volatile uint16_t hueDiff;
extern std::atomic<uint16_t> totalAudioBumps;
extern std::atomic<uint16_t> audioBumpThreshold;
extern uint16_t maxAudio[AUDIO_HIST_BINS_COUNT];
extern std::atomic<bool> fxBump;
extern std::atomic<bool> fxBroadcastEnabled;
extern std::atomic<uint16_t> speed;
extern std::atomic<uint16_t> curPos;

namespace fx {
    extern CRGB leds[NUM_PIXELS];
    extern CRGBArray<PIXEL_BUFFER_SPACE> frame;
    extern CRGBSet tpl;
    extern CRGBSet others;
    extern CRGBSet ledSet;
    extern uint16_t stripShuffleIndex[NUM_PIXELS];
    extern CRGBPalette16 palette;
    extern CRGBPalette16 targetPalette;
    extern OpMode mode;
    extern bool dirFwd;
    extern int32_t dist;
}

#define EFFECT_FACTORY(EffectClass) \
    []() -> LedEffect* { return new EffectClass(); }

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
