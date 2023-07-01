//
// Copyright 2023 by Dan Luca. All rights reserved
//

#ifndef LIGHTFX_EFX_SETUP_H
#define LIGHTFX_EFX_SETUP_H

#include <FastLED.h>
#include <ArduinoJson.h>
#include "config.h"
#include "PaletteFactory.h"

#define capd(x, d) ((x<d)?d:x)
#define capu(x, u) ((x>u)?u:x)
#define capr(x, d, u) (capu(capd(x,d),u))
#define inr(x, d, u) ((x>=d)&&(x<u))
#define inc(x, i, u) ((x+i)%u)
#define arrSize(A) (sizeof(A) / sizeof((A)[0]))

#define MAX_EFFECTS_COUNT   256

extern CRGB leds[NUM_PIXELS];

typedef void (*setupFunc)();

void setupStateLED();

void stateLED(CRGB color);

void ledStripInit();

void showFill(const CRGB frame[], uint szFrame);

void shiftRight(CRGB arr[], uint szArr, uint pos);

void shiftLeft(CRGB arr[], uint szArr, uint pos);

void shuffleIndexes(uint array[], uint szArray);

void offTrailColor(CRGB arr[], uint x);

void setTrailColor(CRGB arr[], uint x, CRGB color, uint8_t dotBrightness, uint8_t trailBrightness);

void copyArray(CRGB *src, CRGB *dest, size_t length);

void copyArray(const CRGB *src, uint srcOfs, CRGB *dest, uint destOfs, size_t length);

bool isAnyLedOn(CRGB arr[], uint szArray, CRGB backg);

void fillArray(CRGB *src, size_t szSrc, CRGB color);

void fillArray(const CRGB *src, size_t srcLength, CRGB *array, size_t arrLength, uint arrOfs = 0);

void pushFrame(const CRGB frame[], uint szFrame, uint ofsDest = 0, bool repeat = false);

CRGB *mirrorLow(CRGB array[], uint szArray);

CRGB *mirrorHigh(CRGB array[], uint szArray);

CRGB *reverseArray(CRGB array[], uint szArray);

CRGB *cloneArray(const CRGB src[], CRGB dest[], size_t length);

void fxa_register();

void fxb_register();

void fxc_register();

void fxd_register();

void fxe_register();

void fxf_register();

void fxh_register();

void fxi_register();

void fxj_register();

void fxk_register();

void fx_setup();

void fx_run();

// Classes - OOP rules for embedded!

//base class/interface for all effects
class LedEffect {
protected:
    uint registryIndex = 0;
public:
    virtual void setup() = 0;

    virtual void loop() = 0;

    virtual const char *description() = 0;

    virtual const char *name() = 0;

    virtual void describeConfig(JsonArray &json) = 0;

    void baseConfig(JsonObject &json);

    uint getRegistryIndex() const;

    virtual ~LedEffect() = default;     // Destructor
};

class EffectRegistry {
private:
    LedEffect *effects[MAX_EFFECTS_COUNT];
    uint currentEffect = 0;
    uint effectsCount = 0;
    uint lastEffectRun = 0;
    bool autoSwitch = true;
public:
    EffectRegistry() : effects() {};

    LedEffect *getCurrentEffect() const;

    LedEffect *getEffect(uint index) const;

    uint nextEffectPos(uint efx);

    uint nextEffectPos();

    uint curEffectPos() const;

    uint nextRandomEffectPos();

    uint registerEffect(LedEffect *effect);

    void setup();

    void loop();

    void describeConfig(JsonArray &json);

    void autoRoll(bool switchType = true);

    bool isAutoRoll() const;
};

extern EffectRegistry fxRegistry;

const setupFunc categorySetup[] = {fxa_register, fxb_register, fxc_register, fxd_register, fxe_register, fxf_register, fxh_register, fxi_register, fxj_register, fxk_register};


#endif //LIGHTFX_EFX_SETUP_H
