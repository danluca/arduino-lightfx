//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXD_H
#define LIGHTFX_FXD_H

#include "efx_setup.h"

// Define variables used by the sequences.
extern uint8_t fade;
extern uint8_t hue;
extern uint8_t delta;
extern uint8_t saturation;
extern uint hueDiff;
extern uint8_t dotBpm;
extern uint8_t brightness;
extern uint8_t speed;
extern CRGBPalette16 palette;
extern CRGBPalette16 targetPalette;
extern const uint8_t maxChanges;

class FxD1 : public LedEffect {
public:
    FxD1();

    void setup() override;

    void loop() override;

    const char *description() const override;

    void describeConfig(JsonArray &json) const override;

    const char *name() const override;

    void ChangeMe();

    void confetti();
};

class FxD2 : public LedEffect {
public:
    FxD2();

    void setup() override;

    void loop() override;

    void describeConfig(JsonArray &json) const override;

    const char *name() const override;

    const char *description() const override;

    void dot_beat();
};

class FxD3 : public LedEffect {
public:
    FxD3();

    void setup() override;

    void loop() override;

    void describeConfig(JsonArray &json) const override;

    const char *description() const override;

    const char *name() const override;

    void plasma();
};

class FxD4 : public LedEffect {
public:
    FxD4();

    void setup() override;

    void loop() override;

    const char *description() const override;

    const char *name() const override;

    void describeConfig(JsonArray &json) const override;

    void rainbow_march();

    void update_params(uint8_t slot);
};


struct ripple {
    uint8_t rpBright;
    uint8_t color;
    uint16_t center;
    uint8_t rpFade;                                          // low value - slow fade to black
    uint8_t step;

    void Move() {
        if (step == 0) {
            leds[center] = ColorFromPalette(palette, color, rpBright, LINEARBLEND);
        } else if (step < 12) {
            int x = (center + step) % NUM_PIXELS;
            x = (center + step) >= NUM_PIXELS ? (NUM_PIXELS - x -1) : x;        // we want the "wave" to bounce back from the end, rather than start from the other end
            leds[x] += ColorFromPalette(palette, color+16, rpBright / step * 2, LINEARBLEND);       // Simple wrap from Marc Miller
            x = abs(center - step) % NUM_PIXELS;
            leds[x] += ColorFromPalette(palette, color+16, rpBright / step * 2, LINEARBLEND);
        }
        step ++;                                                         // Next step.
    } // Move()

    void Fade() {
        uint16_t lowEndRipple = capd(center - step, 0);
        uint16_t upEndRipple = capu(center + step, NUM_PIXELS);
        for (uint16_t x = lowEndRipple; x < upEndRipple; x++)
            leds[x].fadeToBlackBy(rpFade);
    }

    bool Alive() {
        return step < 42;
    }

    void Init() {
        center = random8(NUM_PIXELS/8, NUM_PIXELS-NUM_PIXELS/8);          // Avoid spawning too close to edge.
        rpBright = random8(192, 255);                                   // upper range of localBright
        color = random8();
        rpFade = random8(96, 176);
        step = 0;
    } // Init()

}; // struct ripple
typedef struct ripple Ripple;

class FxD5 : public LedEffect {
public:
    FxD5();

    void setup() override;

    void loop() override;

    const char *description() const override;

    const char *name() const override;

    void describeConfig(JsonArray &json) const override;

    void ripples();

protected:
    static const uint8_t maxRipples = 7;
    Ripple ripplesData[maxRipples];


};


#endif //LIGHTFX_FXD_H
