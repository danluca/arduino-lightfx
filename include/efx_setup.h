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
#define incr(x, i, u) x=(x+i)%(u)
#define arrSize(A) (sizeof(A) / sizeof((A)[0]))
#define qsubd(x, b) ((x>b)?b:0)                               // Clip. . . . A digital unsigned subtraction macro. if result <0, then x=0. Otherwise, x=b.
#define qsuba(x, b) ((x>b)?x-b:0)                             // Level shift. . . Unsigned subtraction macro. if result <0, then x=0. Otherwise x=x-b.

#define MAX_EFFECTS_COUNT   256

const uint16_t turnOffSeq[] = {1, 1, 2, 2, 2, 3, 3, 3, 5, 5, 5, 7, 7, 7, 7, 10};

extern const uint8_t dimmed;
extern const uint16_t FRAME_SIZE;
extern const uint8_t flameBrightness;
extern const uint8_t sparkBrightness;
extern const CRGB BKG;
extern const uint8_t maxChanges;
enum OpMode { TurnOff, Chase };
extern CRGB leds[NUM_PIXELS];
extern CRGBArray<NUM_PIXELS> frame;
extern CRGBSet tpl;
extern CRGBSet others;
extern uint16_t stripShuffleIndex[NUM_PIXELS];
extern CRGBPalette16 palette;
extern CRGBPalette16 targetPalette;
extern OpMode mode;
extern uint8_t brightness;
extern uint8_t colorIndex;
extern uint8_t lastColorIndex;
extern uint8_t fade;
extern uint8_t hue;
extern uint8_t dotBpm;
extern uint8_t saturation;
extern uint8_t delta;
extern uint8_t twinkrate;
extern uint16_t szStack;
extern uint16_t hueDiff;
extern TBlendType currentBlending;
extern bool dirFwd;
extern int8_t rot;
extern int32_t dist;
extern bool randhue;
extern volatile bool fxBump;
extern volatile uint16_t speed;
extern volatile uint16_t curPos;


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

void shiftRight(CRGBSet &set, CRGB feedLeft, Viewport vwp = (Viewport)0, uint16_t pos = 1);
void loopRight(CRGBSet &set, Viewport vwp = (Viewport)0, uint16_t pos = 1);

void shiftLeft(CRGBSet &set, CRGB feedRight, Viewport vwp = (Viewport) 0, uint16_t pos = 1);

void shuffleIndexes(uint16_t array[], uint16_t szArray);

void copyArray(const CRGB *src, CRGB *dest, uint16_t length);

void copyArray(const CRGB *src, uint16_t srcOfs, CRGB *dest, uint16_t destOfs, uint16_t length);

bool isAnyLedOn(CRGB arr[], uint16_t szArray, CRGB backg);

void fillArray(const CRGB *src, uint16_t srcLength, CRGB *array, uint16_t arrLength, uint16_t arrOfs = 0);

void replicateSet(const CRGBSet& src, CRGBSet& dest);

bool turnOffWipe(bool rightDir = false);


CRGB *mirrorLow(CRGB array[], uint16_t szArray);

CRGB *mirrorHigh(CRGB array[], uint16_t szArray);

bool turnOffSpots();

void resetGlobals();

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
void fx_setup();

void fx_run();

void saveState();

void readState();

size_t readTextFile(const char *fname, String *s);
size_t writeTextFile(const char *fname, String *s);
bool removeFile(const char *fname);

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

    uint16_t getRegistryIndex() const;

    virtual ~LedEffect() = default;     // Destructor
};

class EffectRegistry {
private:
    LedEffect *effects[MAX_EFFECTS_COUNT];
    uint16_t currentEffect = 0;
    uint16_t effectsCount = 0;
    uint16_t lastEffectRun = 0;
    bool autoSwitch = true;
public:
    EffectRegistry() : effects() {};

    LedEffect *getCurrentEffect() const;

    LedEffect *getEffect(uint16_t index) const;

    uint16_t nextEffectPos(uint16_t efx);

    uint16_t nextEffectPos();

    uint16_t curEffectPos() const;

    uint16_t nextRandomEffectPos();

    uint16_t registerEffect(LedEffect *effect);

    void setup();

    void loop();

    void describeConfig(JsonArray &json);

    void autoRoll(bool switchType = true);

    bool isAutoRoll() const;
};

extern EffectRegistry fxRegistry;

const setupFunc categorySetup[] = {FxA::fxRegister, FxB::fxRegister, FxC::fxRegister, FxD::fxRegister, FxE::fxRegister, FxF::fxRegister, FxH::fxRegister, FxI::fxRegister, FxJ::fxRegister, FxK::fxRegister};


#endif //LIGHTFX_EFX_SETUP_H
