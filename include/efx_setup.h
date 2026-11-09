//
// Copyright by Dan Luca. All rights reserved
//
#pragma once
#ifndef LIGHTFX_EFX_SETUP_H
#define LIGHTFX_EFX_SETUP_H

#include <FastLED.h>
#include <atomic>
#include "LedEffect.h"
#include "EffectRegistry.h"
#include "config.h"
#include "board_buffers.h"
#include "global.h"
#include "PaletteFactory.h"
#include "constants.hpp"
#include "fxutil.h"
#include "log.h"

using namespace fx;

typedef void (*setupFunc)();

extern volatile uint8_t brightness;
extern std::atomic<uint8_t> stripBrightness;
extern std::atomic<bool> stripBrightnessLocked;
extern volatile uint8_t colorIndex;
extern volatile uint8_t lastColorIndex;
extern volatile uint8_t fade;
extern volatile uint8_t hue;
extern volatile uint8_t dotBpm;
extern volatile uint8_t saturation;
extern volatile uint8_t delta;
extern volatile uint16_t hueDiff;
extern std::atomic<bool> fxBroadcastEnabled;
extern std::atomic<uint16_t> speed;
extern std::atomic<uint16_t> curPos;
extern String masterBoardName;

namespace fx {
    extern CRGB *leds;
    extern CRGBSet &frame;
    extern CRGBSet &tpl;
    extern CRGBSet &others;
    extern CRGBSet &ledSet;
    ScratchView<uint16_t> effectScratchWords(size_t offset, size_t count);
    ScratchView<uint8_t> effectScratchBytes(size_t offset, size_t count);
    extern uint16_t *stripShuffleIndex;
    extern CRGBPalette16 palette;
    extern CRGBPalette16 targetPalette;
    extern OpMode mode;
    extern bool dirFwd;
    extern int32_t dist;
}

#define EFFECT_FACTORY(EffectClass) \
    ([]() -> LedEffect* { return new EffectClass(); })

bool ledStripInit();
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
