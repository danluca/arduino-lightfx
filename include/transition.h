// Copyright (c) 2023,2024 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_TRANSITION_H
#define ARDUINO_LIGHTFX_TRANSITION_H

#include <Arduino.h>
#include "global.h"
#include <deque>

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
    void resetRandomBars();
    //fade off effects
    bool offSpots();
    bool offWipe(bool rightDir = true);
    bool offHalfWipe(bool inward = true);
    bool offSplit(bool outward = true);
    bool offRandomBars(bool rightDir = true);
    bool offFade();

protected:
    static const uint8_t effectsCount = 6;  //number of 'offXYZ' methods
    uint sel=0;
    uint8_t prefFx = 0;
    //offSpots variables
    uint16_t offSpotShuffleOffset;
    uint16_t offPosIndex;
    uint16_t offSpotSegSize;
    //offRandomBars variables
    std::deque<uint8_t> randomBarSegs;
};

extern EffectTransition transEffect;

#endif //ARDUINO_LIGHTFX_TRANSITION_H
