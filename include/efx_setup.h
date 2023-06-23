//
// Created by Dan on 06.21.2023.
//

#ifndef LIGHTFX_EFX_SETUP_H
#define LIGHTFX_EFX_SETUP_H

#include <FastLED.h>
#include "config.h"

#define capd(x,d) ((x<d)?d:x)
#define capu(x,u) ((x>u)?u:x)
#define capr(x,d,u) (capu(capd(x,d),u))
#define inr(x,d,u) ((x>=d)&&(x<u))
#define inc(x,i,u) ((x+i)%u)
#define arrSize(A) (sizeof(A) / sizeof((A)[0]))

#define MAX_EFFECTS_COUNT   256

extern CRGB leds[NUM_PIXELS];

//base class/interface for all effects - boy, venturing into OOP in embedded environment!!
class LedEffect {
public:
    virtual void setup() = 0;
    virtual void loop() = 0;
    virtual const char* description() = 0;

    virtual ~LedEffect() {}     // Destructor
};

// Effectx setup and run functions - two arrays of same(!) size
//void (*const setupFunc[])() = {fxa01_setup, fxa01a_setup, fxa02_setup, fxa03_setup, fxa04_setup,  fxb01_setup, fxb02_setup, fxb03_setup, fxb04_setup, fxb05_setup, fxb06_setup, fxb07_setup, fxb08_setup, fxc01_setup, fxc02_setup, fxd01_setup, fxd02_setup, fxe01_setup, fxe02_setup, fxh01_setup, fxh02_setup};
//void (*const fxrunFunc[])() = {fxa01_run,   fxa01a_run,   fxa02_run,   fxa03_run,   fxa04_run,    fxb01_run,   fxb02_run,   fxb03_run,   fxb04_run,   fxb05_run,   fxb06_run,   fxb07_run,   fxb08_run,   fxc01_run,   fxc02_run,   fxd01_run,   fxd02_run,   fxe01_run,   fxe02_run,   fxh01_run,   fxh02_run};

// current effect
extern ulong fxSwitchTime;
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
void copyArray(CRGB* src, CRGB* dest, size_t length);
void copyArray(const CRGB* src, uint srcOfs, CRGB* dest, uint destOfs, size_t length);
bool isAnyLedOn(CRGB arr[], uint szArray, CRGB backg);
void fillArray(CRGB* src, size_t szSrc, CRGB color);
void fillArray(const CRGB* src, size_t srcLength, CRGB* array, size_t arrLength, uint arrOfs=0);
void pushFrame(const CRGB frame[], uint szFrame, uint ofsDest=0, bool repeat=false);
CRGB* mirrorLow(CRGB array[], uint szArray);
CRGB* mirrorHigh(CRGB array[], uint szArray);
CRGB* reverseArray(CRGB array[], uint szArray);
CRGB* cloneArray(const CRGB src[], CRGB dest[], size_t length);
void fx_setup();
void fx_run();

class EffectRegistry {
private:
    LedEffect* effects[MAX_EFFECTS_COUNT];
    uint currentEffect = 0;
    uint effectsCount = 0;
    uint lastEffectRun = 0;
public:
    LedEffect * getCurrentEffect();
    uint nextEffectPos(uint efx);
    uint nextEffectPos();
    uint curEffectPos();
    uint randomNextEffectPos();
    EffectRegistry* registerEffect(LedEffect* effect);
    void setup();
    void loop();
};

extern EffectRegistry fxRegistry;

#endif //LIGHTFX_EFX_SETUP_H
