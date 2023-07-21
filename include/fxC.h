//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXC_H
#define LIGHTFX_FXC_H

#include "efx_setup.h"

extern uint8_t brightness;
extern CRGBPalette16 palette;
extern CRGBPalette16 targetPalette;
extern const CRGB BKG;

class FxC1 : public LedEffect {
private:
    CRGB leds2[NUM_PIXELS];
    CRGB leds3[NUM_PIXELS];

public:
    FxC1();

    void setup() override;

    void loop() override;

    const char *description() override;

    void animationA();

    void animationB();

    void describeConfig(JsonArray &json) override;

    const char *name() override;
};

class FxC2 : public LedEffect {
public:
    FxC2();

    void setup() override;

    void loop() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;

    const char *description() override;
};

class FxC3 : public LedEffect {
public:
    FxC3();

    void setup() override;

    void loop() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;

    const char *description() override;
};

class FxC4 : public LedEffect {
public:
    FxC4();

    void setup() override;

    void loop() override;

    void describeConfig(JsonArray &json) override;

    const char *description() override;

    const char *name() override;
};

class FxC5 : public LedEffect {
public:
    FxC5();

    void setup() override;

    void loop() override;

    void describeConfig(JsonArray &json) override;

    const char *description() override;

    const char *name() override;

    void changeParams();
    void matrix();

protected:
    int      delay = 50;                                     // A delay value for the sequence(s)
    uint8_t  palIndex = 95;
    uint8_t  sat = 255;
    bool     fwd = true;
    bool     huerot = false;                                     // Does the hue rotate? 1 = yes
    uint8_t  bgclr = 0;
    uint8_t  bgbri = 0;
};

#endif //LIGHTFX_FXC_H
