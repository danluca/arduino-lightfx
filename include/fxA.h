//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXA_H
#define LIGHTFX_FXA_H

#include "efx_setup.h"

const uint MAX_DOT_SIZE = 16;
const uint8_t dimmed = 20;
const uint FRAME_SIZE = 50;
enum OpMode {
    TurnOff, Chase
};

extern const CRGB BKG;
extern CRGBPalette16 palette;
extern uint8_t brightness;
extern uint8_t colorIndex;
extern uint8_t lastColorIndex;
extern uint16_t szStack;
extern OpMode mode;
extern CRGBArray<NUM_PIXELS> frame;
extern volatile uint16_t speed;
extern volatile uint16_t curPos;


void stack(CRGB color, CRGB dest[], uint16_t stackStart, uint16_t szStackSeg);

uint16_t fxa_incStackSize(int16_t delta, uint16_t max);

uint16_t fxa_stackAdjust(CRGB array[], uint16_t szArray, uint16_t szStackSeg);

void reset();

class FxA1 : public LedEffect {
public:
    FxA1();

    void setup() override;

    void loop() override;

    void describeConfig(JsonArray &json) override;

    const char *description() override;

    const char *name() override;

protected:
    CRGB dot[MAX_DOT_SIZE]{};
    void makeDot(CRGB color, uint16_t szDot);
    uint8_t szSegment = 3;
    uint8_t szStackSeg = 1;
    bool bConstSpeed = true;
};

class FxA2 : public LedEffect {
public:
    FxA2();

    void setup() override;

    void loop() override;

    const char *description() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;

protected:
    enum Movement {forward, pause, backward};
    CRGB dot[MAX_DOT_SIZE]{};
    void makeDot();
    uint8_t szSegment = 5;
    Movement movement = forward;
    uint8_t spacing = 1;
};

class FxA3 : public LedEffect {
public:
    FxA3();

    void setup() override;

    void loop() override;

    const char *description() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;

protected:
    CRGB dot[MAX_DOT_SIZE]{};
    void makeDot(CRGB color, uint16_t szDot);
    uint8_t szSegment = 5;
    uint8_t szStackSeg = 3;
    bool bFwd = true;
};

class FxA4 : public LedEffect {
public:
    FxA4();

    void setup() override;

    void loop() override;

    const char *description() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;

protected:
    CRGB dot[MAX_DOT_SIZE]{};
    void makeDot(CRGB color, uint16_t szDot);
    uint8_t szSegment = 5;
    uint8_t szStackSeg = 3;
    uint8_t spacing = 7;
    CRGB curBkg;
};

class FxA5 : public LedEffect {
public:
    FxA5();

    void setup() override;

    void loop() override;

    const char *description() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;

protected:
    CRGBSet ovr;
    CRGB background;
    void makeFrame();
};

#endif //LIGHTFX_FXA_H
