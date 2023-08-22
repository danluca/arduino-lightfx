//
// Copyright 2023 by Dan Luca. All rights reserved
//

#ifndef LIGHTFX_EFX_SETUP_H
#define LIGHTFX_EFX_SETUP_H

#include "mic.h"
#include <FastLED.h>
#include <ArduinoJson.h>
#include "config.h"
#include "PaletteFactory.h"

#define capd(x, d) ((x<d)?d:x)
#define capu(x, u) ((x>u)?u:x)
#define capr(x, d, u) (capu(capd(x,d),u))
#define inr(x, d, u) ((x>=d)&&(x<u))
#define inc(x, i, u) ((x+i)%u)
#define incr(x, i, u) (x=(x+i)%u)
#define arrSize(A) (sizeof(A) / sizeof((A)[0]))
#define qsubd(x, b) ((x>b)?b:0)                               // Clip. . . . A digital unsigned subtraction macro. if result <0, then x=0. Otherwise, x=b.
#define qsuba(x, b) ((x>b)?x-b:0)                             // Level shift. . . Unsigned subtraction macro. if result <0, then x=0. Otherwise x=x-b.

#define MAX_EFFECTS_COUNT   256

const uint16_t turnOffSeq[] = {1, 1, 2, 2, 2, 3, 3, 3, 5, 5, 5, 7, 7, 7, 7, 10};

extern CRGB leds[NUM_PIXELS];
extern const CRGB BKG;
extern uint16_t stripShuffleIndex[NUM_PIXELS];
extern volatile bool fxBump;

typedef void (*setupFunc)();

/**
 * Viewport definition packed as a 4-byte unsigned integer - uint32_t
 */
struct Viewport {
    uint16_t low;
    uint16_t high;

    explicit Viewport(uint16_t high);
    Viewport(uint16_t low, uint16_t high);
    uint16_t size() const;
};



void setupStateLED();

void stateLED(CRGB color);

void ledStripInit();

void fsInit();

void showFill(const CRGB frame[], uint16_t szFrame);

void shiftRight(CRGB arr[], uint16_t szArr, Viewport vwp, uint16_t pos = 1, CRGB feedLeft = BKG);
void shiftRight(CRGBSet &set, CRGB feedLeft, Viewport vwp = (Viewport)0, uint16_t pos = 1);

void shiftLeft(CRGB arr[], uint16_t szArr, Viewport vwp, uint16_t pos = 1, CRGB feedRight = BKG);
void shiftLeft(CRGBSet &set, CRGB feedRight, Viewport vwp = (Viewport) 0, uint16_t pos = 1);

void shuffleIndexes(uint16_t array[], uint16_t szArray);

void offTrailColor(CRGB arr[], uint16_t x);

void setTrailColor(CRGB arr[], uint16_t x, CRGB color, uint8_t dotBrightness, uint8_t trailBrightness);

void copyArray(const CRGB *src, CRGB *dest, uint16_t length);

void copyArray(const CRGB *src, uint16_t srcOfs, CRGB *dest, uint16_t destOfs, uint16_t length);

bool isAnyLedOn(CRGB arr[], uint16_t szArray, CRGB backg);

void fillArray(const CRGB *src, uint16_t srcLength, CRGB *array, uint16_t arrLength, uint16_t arrOfs = 0);

void replicateSet(const CRGBSet& src, CRGBSet& dest);

void pushFrame(const CRGB frame[], uint16_t szFrame, uint16_t ofsDest = 0, bool repeat = false);

bool turnOffJuggle();


CRGB *mirrorLow(CRGB array[], uint16_t szArray);

CRGB *mirrorHigh(CRGB array[], uint16_t szArray);

CRGB *reverseArray(CRGB array[], uint16_t szArray);

CRGB *cloneArray(const CRGB src[], CRGB dest[], uint16_t length);

bool turnOff();


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

void saveState();

//base class/interface for all effects
class LedEffect {
protected:
    uint registryIndex = 0;
public:
    virtual void setup() = 0;

    virtual void loop() = 0;

    virtual const char *description() const = 0;

    virtual const char *name() const = 0;

    virtual void describeConfig(JsonArray &json) const = 0;

    void baseConfig(JsonObject &json) const;

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
