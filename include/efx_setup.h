//
// Created by Dan on 06.21.2023.
//

#ifndef LIGHTFX_EFX_SETUP_H
#define LIGHTFX_EFX_SETUP_H

#include <FastLED.h>
#include <ArduinoJson.h>
#include "config.h"

#define capd(x, d) ((x<d)?d:x)
#define capu(x, u) ((x>u)?u:x)
#define capr(x, d, u) (capu(capd(x,d),u))
#define inr(x, d, u) ((x>=d)&&(x<u))
#define inc(x, i, u) ((x+i)%u)
#define arrSize(A) (sizeof(A) / sizeof((A)[0]))

#define MAX_EFFECTS_COUNT   256

extern CRGB leds[NUM_PIXELS];

typedef void (*setupFunc)();

//base class/interface for all effects - boy, venturing into OOP in embedded environment!!
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

    uint getRegistryIndex();

    virtual ~LedEffect() = default;     // Destructor
};

// current effect
extern bool autoSwitch;


void setupStateLED();

void stateLED(CRGB color);

void ledStripInit();

void showFill(const CRGB frame[], uint szFrame);

void shiftRight(CRGB arr[], int szArr, uint pos);

void shiftLeft(CRGB arr[], int szArr, uint pos);

void shuffleIndexes(int array[], uint szArray);

void offTrailColor(CRGB arr[], int x);

void setTrailColor(CRGB arr[], int x, CRGB color, uint8_t dotBrightness, uint8_t trailBrightness);

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

void fxa_setup();

void fxb_setup();

void fxc_setup();

void fxd_setup();

void fxe_setup();

void fxf_setup();

void fxh_setup();

void fx_setup();

void fx_run();

class EffectRegistry {
private:
    LedEffect *effects[MAX_EFFECTS_COUNT];
    uint currentEffect = 0;
    uint effectsCount = 0;
    uint lastEffectRun = MAX_EFFECTS_COUNT; //initialized on purpose to be different than current effect on the first run; causes proper effect's setup to be invoked.
public:
    LedEffect *getCurrentEffect();

    uint nextEffectPos(uint efx);

    uint nextEffectPos();

    uint curEffectPos();

    uint nextRandomEffectPos();

    uint registerEffect(LedEffect *effect);

    void setup();

    void loop();

    void describeConfig(JsonArray &json);
};

extern EffectRegistry fxRegistry;

const setupFunc categorySetup[] = {fxa_setup, fxb_setup, fxc_setup, fxd_setup, fxe_setup, fxf_setup, fxh_setup};


#endif //LIGHTFX_EFX_SETUP_H
