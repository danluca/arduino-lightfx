// Copyright (c) 2023,2025 by Dan Luca. All rights reserved.
//

#include "transition.h"
#include "efx_setup.h"

// EffectTransition - we have 5 distinct off effects
void EffectTransition::setup() {
    prefFx = 0;     //no preference - i.e. automatic from sel
    sel = random8() % 10;
    if (randomBarSegs.empty())
        resetRandomBars();
}

void EffectTransition::resetRandomBars() {
    randomBarSegs.clear();
    uint16_t sum = 0;
    while (sum < NUM_PIXELS) {
        uint8_t szSeg = random8(3, 10);
        randomBarSegs.push_front(szSeg);
        sum += szSeg;
    }
    if (sum > NUM_PIXELS)
        randomBarSegs.front() -= (sum-NUM_PIXELS);
}

bool EffectTransition::transition() {
    switch (uint8_t effect = prefFx ? (prefFx-1) : sel/2) {
        case 0: return offSpots();
        case 1: return offWipe(sel % 2);
        case 2: return offFade();
        case 3: return offSplit(sel % 2);
        case 4: return offRandomBars(sel % 2);
        case 5: return offHalfWipe(sel % 2);
        default:
            return offFade();
    }
}

void EffectTransition::prepare(uint selector) {
    sel = selector % (effectsCount*2);    //twice the amount of fade out effects - allows for 2 variations within each effect
    prefFx = (selector >> 8) & 0xFF;
    if (prefFx) {
        prefFx = (prefFx % effectsCount) + 1;
    }

    //offSpots prepare
    offSpotShuffleOffset = 0;
    offPosIndex = 0;
    offSpotSegSize = turnOffSeq[offPosIndex];
    fade = random8(42, 110);
}

uint EffectTransition::selector() const {
    return sel;
}

/**
 * Turns off entire strip by random spots, in increasing size until all LEDs are off
 * <p>This function needs called repeatedly until it returns true</p>
 * @return true if all LEDs are off, false otherwise
 */
bool EffectTransition::offSpots() {
    bool allOff = false;

    EVERY_N_MILLIS(30) {
        uint8_t ledsOn = 0;
        for (uint16_t x = 0; x < offSpotSegSize; x++) {
            const uint16_t xled = stripShuffleIndex[(offSpotShuffleOffset + x) % NUM_PIXELS];
            FastLED.leds()[xled].fadeToBlackBy(fade);
            if (FastLED.leds()[xled].getLuma() < 4)
                FastLED.leds()[xled] = BKG;
            else
                ledsOn++;
        }

        FastLED.show(stripBrightness);
        if (ledsOn == 0) {
            offSpotShuffleOffset = inc(offSpotShuffleOffset, offSpotSegSize, NUM_PIXELS);  //need to increment with szOffSpot before advancing it
            offPosIndex = inc(offPosIndex, 1, arrSize(turnOffSeq));
            offSpotSegSize = turnOffSeq[offPosIndex];
            fade = random8(42, 110);
        }
    }

    EVERY_N_MILLIS(500) {
        allOff = !isAnyLedOn(FastLED.leds(), FastLED.size(), BKG);
    }

    return allOff;
}

/**
 * Turns off entire strip by leveraging the shift right/left with black feed
 * @param rightDir whether to shift right (default) or left
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offWipe(bool rightDir) {
    bool allOff = false;
    EVERY_N_MILLIS(60) {
        CRGBSet strip(leds, NUM_PIXELS);
        if (rightDir)
            shiftRight(strip, BKG);
        else
            shiftLeft(strip, BKG);
        FastLED.show(stripBrightness);
    }
    EVERY_N_MILLIS(720) {
        allOff = !isAnyLedOn(leds, NUM_PIXELS, BKG);
    }

    return allOff;
}

/**
 * Similar effect with <code>offSplit(bool)</code> but shifting the two half outward or inward - same idea as <code>offWipe(bool)</code>
 * @param inward whether to shift right (default) or left
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offHalfWipe(bool inward) {
    bool allOff = false;
    EVERY_N_MILLIS(60) {
        constexpr uint16_t halfSize = NUM_PIXELS/2;
        CRGBSet stripH1(leds, halfSize);
        CRGBSet stripH2(leds, halfSize, NUM_PIXELS-1);
        if (inward) {
            //inward
            shiftRight(stripH1, BKG);
            shiftLeft(stripH2, BKG);
        } else {
            //outward
            shiftLeft(stripH1, BKG);
            shiftRight(stripH2, BKG);
        }
        FastLED.show(stripBrightness);
    }
    EVERY_N_MILLIS(720) {
        allOff = !isAnyLedOn(leds, NUM_PIXELS, BKG);
    }

    return allOff;
}

/**
 * Turns off entire strip by fading it to black
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offFade() {
    bool allOff = false;
    EVERY_N_MILLIS(50) {
        CRGBSet strip(leds, NUM_PIXELS);
        strip.fadeToBlackBy(32);
        FastLED.show(stripBrightness);
    }
    EVERY_N_MILLIS(500) {
        allOff = !isAnyLedOn(leds, NUM_PIXELS, BKG);
    }
    return allOff;
}

/**
 * Turns off entire strip by splitting in half and wiping off each half outward or inward
 * @param outward whether to split outward (default) - i.e. from center to start/end, or inward - i.e. from ends towards center
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offSplit(bool outward) {
    bool allOff = false;
    EVERY_N_MILLIS(50) {
        constexpr uint16_t halfSize = NUM_PIXELS/2;
        constexpr uint16_t maxIndex = NUM_PIXELS-1;
        const uint16_t offSegSize = 1+offPosIndex/8;
        CRGBSet s1(leds, outward?offPosIndex:qsuba(halfSize-1, offPosIndex), outward?(offPosIndex+offSegSize):qsuba(halfSize-1, offPosIndex+offSegSize));
        CRGBSet s2(leds, outward?(maxIndex-offPosIndex):capu(halfSize+offPosIndex, maxIndex), outward?(maxIndex-offPosIndex-offSegSize):capu(halfSize+offPosIndex+offSegSize, maxIndex));
        s1.nblend(BKG, 120);
        s2.nblend(BKG, 120);
        if (!s1 && !s2)
            offPosIndex+=(1+offSegSize);

        FastLED.show(stripBrightness);
        allOff = offPosIndex >= halfSize;
    }
//    EVERY_N_MILLIS(500) {
//        allOff = !isAnyLedOn(leds, NUM_PIXELS, BKG);
//    }
    return allOff;
}

/**
 * Turns off entire strip by sectioning it in segments of random size and concurrently wiping those off in random directions (left/right)
 * @param rightDir whether turning off happens from left to right (default) or right to left
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offRandomBars(bool rightDir) const {
    bool allOff = false;
    EVERY_N_MILLIS(50) {
        uint16_t ovrSz = 0;
        bool segOff = true;
        for (auto &sz : randomBarSegs) {
            if (CRGBSet seg(leds, rightDir?ovrSz:(ovrSz+sz-1), rightDir?(ovrSz+sz-1):ovrSz); !spreadColor(seg, BKG, 120))
                segOff = false;
            ovrSz+=sz;
        }
        FastLED.show(stripBrightness);
        allOff = segOff;
    }
    return allOff;
}
