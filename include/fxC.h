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
extern const uint8_t maxChanges;
extern uint8_t speed;
extern uint8_t saturation;

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
    uint8_t  palIndex = 95;
    bool     fwd = true;
    bool     hueRot = false;                                     // Does the hue rotate? 1 = yes
    uint8_t  bgClr = 0;
    uint8_t  bgBri = 0;
};

class FxC6 : public LedEffect {
public:
    FxC6();

    void setup() override;

    void loop() override;

    const char *description() override;

    const char *name() override;

    void describeConfig(JsonArray &json) override;

    void one_sine_pal(uint8_t colorIndex);

protected:
    uint8_t allfreq = 32;                                     // You can change the frequency, thus distance between bars.
    int phase = 0;                                            // Phase change value gets calculated.
    uint8_t cutoff = 192;                                     // You can change the cutoff value to display this wave. Lower value = longer wave.
    int delay = 30;                                           // You can change the delay. Also you can change the speed global variable.
    uint8_t bgclr = 0;                                        // A rotating background colour.
    uint8_t bgbright = 10;                                    // Brightness of background colour

};

#endif //LIGHTFX_FXC_H
