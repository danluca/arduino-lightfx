//
// Copyright 2023,2024 by Dan Luca. All rights reserved
//

#ifndef LIGHTFX_EFX_SETUP_H
#define LIGHTFX_EFX_SETUP_H

#include <Arduino.h>
#include "mic.h"
#include "PaletteFactory.h"
#include <ArduinoJson.h>
#include "global.h"
#include "util.h"
#include "transition.h"
#include "config.h"

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


void stateLED(CRGB color);

void ledStripInit();

void shiftRight(CRGBSet &set, CRGB feedLeft, Viewport vwp = (Viewport)0, uint16_t pos = 1);

void loopRight(CRGBSet &set, Viewport vwp = (Viewport)0, uint16_t pos = 1);

void shiftLeft(CRGBSet &set, CRGB feedRight, Viewport vwp = (Viewport) 0, uint16_t pos = 1);

bool spreadColor(CRGBSet &set, CRGB color = BKG, uint8_t gradient = 255);

bool moveBlend(CRGBSet &target, const CRGBSet &segment, fract8 overlay, uint16_t fromPos, uint16_t toPos);
bool areSame(const CRGBSet &lhs, const CRGBSet &rhs);

void shuffleIndexes(uint16_t array[], uint16_t szArray);
void shuffle(CRGBSet &set);

uint16_t easeOutBounce(uint16_t x, uint16_t lim);
uint16_t easeOutQuad(uint16_t x, uint16_t lim);

void copyArray(const CRGB *src, CRGB *dest, uint16_t length);

void copyArray(const CRGB *src, uint16_t srcOfs, CRGB *dest, uint16_t destOfs, uint16_t length);

uint16_t countPixelsBrighter(CRGBSet *set, CRGB backg = BKG);

bool isAnyLedOn(CRGBSet *set, CRGB backg = BKG);

bool isAnyLedOn(CRGB *arr, uint16_t szArray, CRGB backg = BKG);

void fillArray(const CRGB *src, uint16_t srcLength, CRGB *array, uint16_t arrLength, uint16_t arrOfs = 0);

void replicateSet(const CRGBSet& src, CRGBSet& dest);

uint8_t adjustStripBrightness();

void mirrorLow(CRGBSet &set);

void mirrorHigh(CRGBSet &set);

void resetGlobals();

CRGB& setBrightness(CRGB& rgb, uint8_t br);
uint8_t getBrightness(const CRGB& rgb);

inline CHSV toHSV(const CRGB &rgb) { return rgb2hsv_approximate(rgb); }
inline CRGB toRGB(const CHSV &hsv) { CRGB rgb{}; hsv2rgb_rainbow(hsv, rgb); return rgb; }

void blendMultiply(CRGBSet &blendLayer, const CRGBSet &topLayer);
void blendMultiply(CRGB &blendRGB, const CRGB &topRGB);
void blendScreen(CRGBSet &blendLayer, const CRGBSet &topLayer);
void blendScreen(CRGB &blendRGB, const CRGB &topRGB);
void blendOverlay(CRGBSet &blendLayer, const CRGBSet &topLayer);
void blendOverlay(CRGB &blendRGB, const CRGB &topRGB);
CRGB adjustBrightness(CRGB color, uint8_t bright);
void saveState();
void readState();

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

//base class/interface for all effects
class LedEffect {
protected:
    uint registryIndex = 0;
    EffectState state;
    ulong transOffStart = 0;
    const char* const desc;
    char id[LED_EFFECT_ID_SIZE] {};   //this is name of the class, max 5 characters (plus null terminal)
public:
    explicit LedEffect(const char* description);

    virtual void setup();

    virtual void run() = 0;

    virtual bool windDown();

    virtual void windDownPrep();

    virtual bool transitionBreak();

    virtual void transitionBreakPrep();

    virtual void desiredState(EffectState dst);

    virtual void nextState();

    virtual void loop();

    const char *description() const;

    const char *name() const;

    virtual JsonObject& describeConfig(JsonArray &json) const;

    void baseConfig(JsonObject &json) const;

    uint16_t getRegistryIndex() const;

    inline EffectState getState() const {
        return state;
    }

    /**
     * What weight does this effect have when random selection is engaged
     * Subclasses have the opportunity to customize this value by e.g. the current holiday, time, etc., hence changing/reshaping the chances of selecting an effect
     * @return a value between 1 and 255. If returning 0, this effectively removes the effect from random selection.
     */
    virtual inline uint8_t selectionWeight() const {
        return 1;
    }

    virtual ~LedEffect() = default;     // Destructor
};

class EffectRegistry {
private:
    std::deque<LedEffect*> effects;
    FixedQueue<uint16_t, MAX_EFFECTS_HISTORY> lastEffects;
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

    void transitionEffect() const;

    uint16_t registerEffect(LedEffect *effect);

    uint16_t size() const;

    void setup();

    void loop();

    void describeConfig(JsonArray &json);

    void pastEffectsRun(JsonArray &json);

    void autoRoll(bool switchType = true);

    bool isAutoRoll() const;
};

extern EffectRegistry fxRegistry;

const setupFunc categorySetup[] = {FxA::fxRegister, FxB::fxRegister, FxC::fxRegister, FxD::fxRegister, FxE::fxRegister, FxF::fxRegister, FxH::fxRegister, FxI::fxRegister, FxJ::fxRegister, FxK::fxRegister};


#endif //LIGHTFX_EFX_SETUP_H
