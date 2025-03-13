//
// Copyright 2023,2024,2025 by Dan Luca. All rights reserved
//

#ifndef LIGHTFX_EFX_SETUP_H
#define LIGHTFX_EFX_SETUP_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include "fixed_queue.h"
#include "config.h"
#include "global.h"
#include "PaletteFactory.h"
#include "constants.hpp"
#include "log.h"

typedef void (*setupFunc)();

/**
 * Viewport definition packed as a 4-byte unsigned integer - uint32_t
 */
struct Viewport {
    uint16_t low;
    uint16_t high;

    explicit Viewport(uint16_t high);
    Viewport(uint16_t low, uint16_t high);
    [[nodiscard]] uint16_t size() const;
};
enum OpMode { TurnOff, Chase };
enum EffectState:uint8_t {Setup, Running, WindDownPrep, WindDown, TransitionBreakPrep, TransitionBreak, Idle};
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
extern uint8_t twinkRate;
extern uint16_t szStack;
extern uint16_t hueDiff;
extern bool dirFwd;
extern int8_t rot;
extern int32_t dist;
extern bool randHue;
extern uint16_t totalAudioBumps;
extern volatile uint16_t audioBumpThreshold;
extern volatile uint16_t maxAudio[AUDIO_HIST_BINS_COUNT];
extern volatile bool fxBump;
extern volatile bool fxBroadcastEnabled;
extern volatile uint16_t speed;
extern volatile uint16_t curPos;

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

uint16_t countPixelsBrighter(const CRGBSet *set, CRGB backg = BKG);

bool isAnyLedOn(CRGBSet *set, CRGB backg = BKG);

bool isAnyLedOn(const CRGB *arr, uint16_t szArray, CRGB backg = BKG);

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

bool rblend(CRGB &existing, const CRGB &target, fract8 frOverlay);
void blendMultiply(CRGBSet &blendLayer, const CRGBSet &topLayer);
void blendMultiply(CRGB &blendRGB, const CRGB &topRGB);
void blendScreen(CRGBSet &blendLayer, const CRGBSet &topLayer);
void blendScreen(CRGB &blendRGB, const CRGB &topRGB);
void blendOverlay(CRGBSet &blendLayer, const CRGBSet &topLayer);
void blendOverlay(CRGB &blendRGB, const CRGB &topRGB);
CRGB adjustBrightness(CRGB color, uint8_t bright);
void saveFxState();
void readFxState();

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

    [[nodiscard]] const char *description() const;

    [[nodiscard]] const char *name() const;

    virtual void baseConfig(JsonObject &json) const;

    [[nodiscard]] uint16_t getRegistryIndex() const;

    [[nodiscard]] inline EffectState getState() const { return state; }

    /**
     * What weight does this effect have when random selection is engaged
     * Subclasses have the opportunity to customize this value by e.g. the current holiday, time, etc., hence changing/reshaping the chances of selecting an effect
     * @return a value between 1 and 255. If returning 0, this effectively removes the effect from random selection.
     */
    [[nodiscard]] virtual inline uint8_t selectionWeight() const {
        return 1;
    }

    virtual ~LedEffect() = default;     // Destructor
};

class EffectRegistry {
    std::deque<LedEffect*> effects;
    FixedQueue<uint16_t, MAX_EFFECTS_HISTORY> lastEffects;
    uint16_t currentEffect = 0;
    uint16_t effectsCount = 0;
    uint16_t lastEffectRun = 0;
    uint16_t sleepEffect = 0;
    bool autoSwitch = true;
    bool sleepState = false;
    bool sleepModeEnabled = false;

public:
    EffectRegistry() = default;

    [[nodiscard]] LedEffect *getCurrentEffect() const;

    [[nodiscard]] LedEffect *getEffect(uint16_t index) const;

    uint16_t nextEffectPos(uint16_t efx);

    uint16_t nextEffectPos(const char* id);

    uint16_t nextEffectPos();

    [[nodiscard]] uint16_t curEffectPos() const;

    uint16_t nextRandomEffectPos();

    void transitionEffect() const;

    uint16_t registerEffect(LedEffect *effect);

    LedEffect* findEffect(const char* id) const;

    [[nodiscard]] uint16_t size() const;

    void setup() const;

    void loop();

    void describeConfig(const JsonArray &json) const;

    void pastEffectsRun(const JsonArray &json);

    void autoRoll(bool switchType = true);

    [[nodiscard]] bool isAutoRoll() const;

    [[nodiscard]] bool isSleepEnabled() const;

    void enableSleep(bool bSleep);

    [[nodiscard]] bool isAsleep() const;

    void setSleepState(bool sleepFlag);
    friend void readFxState();
    friend void saveFxState();
};

extern EffectRegistry fxRegistry;

extern const setupFunc categorySetup[];


#endif //LIGHTFX_EFX_SETUP_H
