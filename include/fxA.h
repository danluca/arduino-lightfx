//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXA_H
#define LIGHTFX_FXA_H

#include "efx_setup.h"

const uint MAX_DOT_SIZE = 16;
const uint8_t dimmed = 20;
const uint FRAME_SIZE = 36;
const uint turnOffSeq[] = {1, 1, 2, 2, 2, 3, 3, 3, 5, 5, 5, 7, 7, 7, 7, 10};
enum OpMode {
    TurnOff, Chase
};

extern const CRGB BKG;
extern CRGBPalette16 palette;
extern uint8_t brightness;
extern uint8_t colorIndex;
extern uint8_t lastColorIndex;
extern volatile uint speed;
extern uint8_t szSegment;
extern uint8_t szStackSeg;
extern uint szStack;
extern bool bConstSpeed;
extern OpMode mode;
extern uint stripShuffleIndex[NUM_PIXELS];
extern CRGB dot[MAX_DOT_SIZE];
extern CRGB frame[NUM_PIXELS];
extern volatile uint curPos;

CRGB *makeDot(CRGB color, uint szDot);

bool isInViewport(int ledIndex, int viewportSize = FRAME_SIZE);

uint validateSegmentSize(uint segSize);

void moveSeg(const CRGB dot[], uint szDot, CRGB dest[], uint lastPos, uint newPos, uint viewport);

void stack(CRGB color, CRGB dest[], uint stackStart);

uint fxa_incStackSize(int delta, uint max);

uint fxa_stackAdjust(CRGB array[], uint szArray);

void moldWindow();

void reset();

bool turnOff();

class FxA1 : public LedEffect {
public:
    FxA1();

    void setup() override;

    void loop() override;

    void describeConfig(JsonArray &json) override;

    const char *description() override;

    const char *name() override;
};

class FxA2 : public LedEffect {
public:
    FxA2();

    void setup() override;

    void loop() override;

    const char *description() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;
};

class FxA3 : public LedEffect {
public:
    FxA3();

    void setup() override;

    void loop() override;

    const char *description() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;
};

class FxA4 : public LedEffect {
public:
    FxA4();

    void setup() override;

    void loop() override;

    const char *description() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;
};

class FxA5 : public LedEffect {
public:
    FxA5();

    void setup() override;

    void loop() override;

    const char *description() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;
};


#endif //LIGHTFX_FXA_H
