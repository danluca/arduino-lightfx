//
// Created by Dan on 06.21.2023.
//

#ifndef LIGHTFX_FXC_H
#define LIGHTFX_FXC_H

#include "efx_setup.h"

// have 2 more independent CRGBs for the sources

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

#endif //LIGHTFX_FXC_H
