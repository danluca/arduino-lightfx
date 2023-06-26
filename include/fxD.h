//
// Created by Dan on 06.21.2023.
//

#ifndef LIGHTFX_FXD_H
#define LIGHTFX_FXD_H

#include "efx_setup.h"

// Define variables used by the sequences.
extern uint8_t fade;
extern uint8_t hue;
extern uint8_t incr;
extern uint8_t saturation;
extern uint hueDiff;
extern uint8_t dotBpm;
extern uint8_t fadeVal;
extern uint8_t brightness;
extern uint8_t speed;


class FxD1 : public LedEffect {
public:
    FxD1();

    void setup() override;

    void loop() override;

    const char *description() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;

    void ChangeMe();

    void confetti();
};

class FxD2 : public LedEffect {
public:
    FxD2();

    void setup() override;

    void loop() override;

    void describeConfig(JsonArray &json) override;

    const char *name() override;

    const char *description() override;

    void dot_beat();
};


#endif //LIGHTFX_FXD_H
