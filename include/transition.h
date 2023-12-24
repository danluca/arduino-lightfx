// Copyright (c) 2023 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_TRANSITION_H
#define ARDUINO_LIGHTFX_TRANSITION_H

#include <Arduino.h>
#include <FastLED.h>
#include <deque>
#include "global.h"

#define SELECTOR_SPOTS  0x0100
#define SELECTOR_WIPE   0x0200
#define SELECTOR_SPLIT  0x0300
#define SELECTOR_RANDOM_BARS    0x0400
#define SELECTOR_FADE   0x0500

/**
 * Class for managing and implementing transitions between effects
 */
class EffectTransition {
public:
    void setup();
    bool transition();
    void prepare(uint selector = 0);
    uint selector() const;
    //fade off effects
    bool offSpots();
    bool offWipe(bool rightDir = true);
    bool offSplit(bool outward = true);
    bool offRandomBars();
    bool offFade();

protected:
    uint sel=0;
    uint8_t prefFx = 0;
    //offSpots variables
    uint16_t led;
    uint16_t xOffSpot;
    uint16_t szOffSpot;
    //offRandomBars variables
    std::deque<uint8_t> randomBarSegs;
};

extern EffectTransition transEffect;

#endif //ARDUINO_LIGHTFX_TRANSITION_H
