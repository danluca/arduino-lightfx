//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXH_H
#define LIGHTFX_FXH_H

#include "efx_setup.h"
#include <vector>

#define FRAMES_PER_SECOND 10

const uint8_t flameBrightness = 180;
const uint8_t sparkBrightness = 255;
extern CRGBPalette16 palette;
extern CRGBPalette16 targetPalette;
extern uint8_t fade;
extern uint8_t hue;
extern uint8_t delta;
extern uint8_t saturation;
extern uint8_t brightness;
extern uint hueDiff;
extern uint8_t speed;
extern CRGBArray<NUM_PIXELS> frame;
extern TBlendType currentBlending;
extern const uint8_t maxChanges;


//
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation,
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe speed of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100
#define COOLING 75

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 150

class FxH1 : public LedEffect {
private:
    const static uint8_t numFires = 3;
    CRGBSet fires[numFires];
    std::vector<CRGB> hMap;
public:
    FxH1();

    void setup() override;

    void loop() override;

    void describeConfig(JsonArray &json) const override;

    const char *name() const override;

    const char *description() const override;

    void Fire2012WithPalette(uint8_t xFire);
};

class FxH2 : public LedEffect {
public:
    FxH2();

    void setup() override;

    void loop() override;

    void describeConfig(JsonArray &json) const override;

    const char *name() const override;

    const char *description() const override;

    static void confetti_pal();

    static void ChangeMe();
};

class FxH3 : public LedEffect {
public:
    FxH3();

    void setup() override;

    void loop() override;

    const char *description() const override;

    void describeConfig(JsonArray &json) const override;

    const char *name() const override;
};


#endif //LIGHTFX_FXH_H
