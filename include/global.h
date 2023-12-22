// Copyright (c) 2023 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_GLOBAL_H
#define ARDUINO_LIGHTFX_GLOBAL_H

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

#define capd(x, d) (((x)<=(d))?(d):(x))
#define capu(x, u) (((x)>=(u))?(u):(x))
#define capr(x, d, u) (capu(capd(x,d),u))
#define inr(x, d, u) (((x)>=(d))&&((x)<(u)))
#define inc(x, i, u) ((x+i)%(u))
#define incr(x, i, u) x=(x+i)%(u)
#define arrSize(A) (sizeof(A) / sizeof((A)[0]))
#define qsubd(x, b) (((x)>(b))?(b):0)                               // Clip. . . . A digital unsigned subtraction macro. if result <0, then x=0. Otherwise, x=b.
#define qsuba(x, b) (((x)>(b))?(x-b):0)                             // Level shift. . . Unsigned subtraction macro. if result <0, then x=0. Otherwise x=x-b.
#define asub(a, b)  (((a)>(b))?(a-b):(b-a))

#define LED_EFFECT_ID_SIZE  6
#define MAX_EFFECTS_HISTORY 20
#define AUDIO_HIST_BINS_COUNT   10

const uint16_t turnOffSeq[] PROGMEM = {1, 1, 2, 2, 2, 3, 3, 3, 5, 5, 5, 7, 7, 7, 7, 10};
extern const char csAutoFxRoll[];
extern const char csStripBrightness[];
extern const char csAudioThreshold[];
extern const char csColorTheme[];
extern const char csAutoColorAdjust[];
extern const char csRandomSeed[];
extern const char csCurFx[];

extern const uint8_t dimmed;
//extern const uint16_t FRAME_SIZE;
extern const CRGB BKG;
extern const uint8_t maxChanges;
enum OpMode { TurnOff, Chase };
enum EffectState {Setup, Running, WindDownPrep, WindDown, TransitionBreakPrep, TransitionBreak, Idle};
extern CRGB leds[NUM_PIXELS];
extern CRGBArray<NUM_PIXELS> frame;
extern CRGBSet tpl;
extern CRGBSet others;
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
extern uint8_t twinkrate;
extern uint16_t szStack;
extern uint16_t hueDiff;
extern bool dirFwd;
extern int8_t rot;
extern int32_t dist;
extern bool randhue;
extern volatile uint16_t audioBumpThreshold;
extern volatile uint16_t maxAudio[AUDIO_HIST_BINS_COUNT];
extern uint16_t totalAudioBumps;
extern float minVcc;
extern float maxVcc;
extern float minTemp;
extern float maxTemp;

extern volatile bool fxBump;
extern volatile uint16_t speed;
extern volatile uint16_t curPos;


#endif //ARDUINO_LIGHTFX_GLOBAL_H
