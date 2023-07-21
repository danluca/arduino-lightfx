//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXE_H
#define LIGHTFX_FXE_H

#include "efx_setup.h"

#define qsubd(x, b) ((x>b)?b:0)                               // Clip. . . . A digital unsigned subtraction macro. if result <0, then x=0. Otherwise, x=b.
#define qsuba(x, b) ((x>b)?x-b:0)                             // Level shift. . . Unsigned subtraction macro. if result <0, then x=0. Otherwise x=x-b.


// Global variables can be changed on the fly.
//uint8_t max_bright = 128;                                      // Overall brightness.

// Palette definitions
extern CRGBPalette16 palette;
extern CRGBPalette16 targetPalette;
extern uint8_t speed;
extern uint8_t brightness;
extern uint8_t fade;
extern uint8_t hue;
extern uint8_t saturation;
extern TBlendType currentBlending;                                // NOBLEND or LINEARBLEND
extern const uint8_t maxChanges;



class FxE1 : public LedEffect {
public:
    FxE1();

    void setup() override;

    void loop() override;

    const char *description() override;

    static void twinkle();

    void describeConfig(JsonArray &json) override;

    const char *name() override;

    static void ChangeMe();
};

class FxE2 : public LedEffect {
public:
    FxE2();

    void setup() override;

    void loop() override;

    const char *description() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;

    void beatwave();
};


#endif //LIGHTFX_FXE_H
