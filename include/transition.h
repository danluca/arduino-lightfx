// Copyright (c) by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_TRANSITION_H
#define ARDUINO_LIGHTFX_TRANSITION_H

#include <Arduino.h>
#include <deque>

#define SELECTOR_SPOTS  0x0100
#define SELECTOR_WIPE   0x0200
#define SELECTOR_FADE   0x0300
#define SELECTOR_SPLIT  0x0400
#define SELECTOR_RANDOM_BARS    0x0500
#define SELECTOR_HALFWIPE   0x0600
#define SELECTOR_EMBER_ASH  0x0700
#define SELECTOR_IMPLOSION  0x0800
#define SELECTOR_SPARKLE_DISSOLVE   0x0900
#define SELECTOR_VENETIAN_SHUTTER   0x0A00
#define SELECTOR_HARMONIC_DAMPING   0x0B00

/**
 * Class for managing and implementing transitions between effects
 */
class EffectTransition {
public:
    bool transition();
    void prepare(uint selector = 0);
    [[nodiscard]] uint selector() const;
    void resetRandomBars();
    //fade off effects
    bool offSpots();
    bool offWipe(bool rightDir = true);
    bool offHalfWipe(bool inward = true);
    bool offSplit(bool outward = true);
    bool offRandomBars(bool rightDir = true) const;
    bool offFade();
    bool offThermalEmberAsh(bool fastCool = false);
    bool offGravitationalImplosion(bool outward = false);
    bool offSparkleEvaporation(bool colorSparkle = false);
    bool offVenetianShutter(bool inward = true);
    bool offHarmonicWaveDamping(bool travelingWave = true);

protected:
    static constexpr uint8_t effectsCount = 11;  //number of 'offXYZ' methods
    uint sel=0;
    uint8_t prefFx = 0;
    //offSpots variables
    uint16_t offSpotShuffleOffset=0;
    uint16_t offPosIndex=0;
    uint16_t offSpotSegSize=0;
    uint8_t transStep=0;
    //offRandomBars variables
    std::deque<uint8_t> randomBarSegs;
};

namespace fx {
    extern EffectTransition transEffect;
}
#endif //ARDUINO_LIGHTFX_TRANSITION_H
