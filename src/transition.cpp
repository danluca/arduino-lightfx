// Copyright (c) 2023 by Dan Luca. All rights reserved.
//

#include "transition.h"
#include "efx_setup.h"

// EffectTransition - we have 5 distinct off effects
void EffectTransition::setup() {
    prefFx = 0;     //no preference - i.e. automatic from sel
    sel = random8() % 10;
    randomBarSegs.clear();
    uint16_t sum = 0;
    while (sum < NUM_PIXELS) {
        uint8_t szSeg = random8(3, 8);
        randomBarSegs.push_front(szSeg);
        sum += szSeg;
    }
    if (sum > NUM_PIXELS)
        randomBarSegs.front() -= (sum-NUM_PIXELS);
}

bool EffectTransition::transition() {
    uint8_t effect = prefFx ? (prefFx-1) : sel/2;
    switch (effect) {
        case 0: return offSpots();
        case 1: return offWipe(sel % 2);
        case 2: return offFade();
        case 3: return offSplit(sel % 2);
        case 4: return offRandomBars();
        default:
            return offFade();
    }
}

void EffectTransition::prepare(uint selector) {
    sel = selector % 10;    //twice the amount of fade out effects - allows for 2 variations within each effect
    prefFx = (selector >> 8) & 0xFF;
    if (prefFx) {
        prefFx = (prefFx % 5) + 1;
    }

    //offSpots prepare
    offSpotShuffleOffset = 0;
    offSpotSeqIndex = 0;
    offSpotSegSize = turnOffSeq[offSpotSeqIndex];
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
            uint16_t xled = stripShuffleIndex[(offSpotShuffleOffset + x) % NUM_PIXELS];
            FastLED.leds()[xled].fadeToBlackBy(fade);
            if (FastLED.leds()[xled].getLuma() < 4)
                FastLED.leds()[xled] = BKG;
            else
                ledsOn++;
        }

        FastLED.show(stripBrightness);
        if (ledsOn == 0) {
            offSpotShuffleOffset = inc(offSpotShuffleOffset, offSpotSegSize, NUM_PIXELS);  //need to increment with szOffSpot before advancing it
            offSpotSeqIndex = inc(offSpotSeqIndex, 1, arrSize(turnOffSeq));
            offSpotSegSize = turnOffSeq[offSpotSeqIndex];
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
 * Turns off entire strip by fading it to black
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offFade() {
    bool allOff = false;
    EVERY_N_MILLIS(50) {
        CRGBSet strip(leds, NUM_PIXELS);
        strip.fadeToBlackBy(11);
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
        const uint16_t halfSize = NUM_PIXELS/2;     //num pixels is an even number, hence both segments below have the same length
        CRGBSet firstSeg(leds, outward?0:(halfSize-1), outward?(halfSize-1):0);
        CRGBSet secondSeg(leds, outward?(NUM_PIXELS-1):halfSize, outward?halfSize:(NUM_PIXELS-1));
        bool first = spreadColor(firstSeg, BKG, 64);
        bool second = spreadColor(secondSeg, BKG, 64);
        FastLED.show(stripBrightness);
        allOff = first && second;
    }
//    EVERY_N_MILLIS(500) {
//        allOff = !isAnyLedOn(leds, NUM_PIXELS, BKG);
//    }
    return allOff;
}

/**
 * Turns off entire strip by sectioning it in segments of random size and concurrently wiping those off in random directions (left/right)
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offRandomBars() {
    bool allOff = false;
    EVERY_N_MILLIS(50) {
        uint16_t ovrSz = 0;
        bool segOff = true;
        for (auto &sz : randomBarSegs) {
            CRGBSet seg(leds, ovrSz, ovrSz+sz-1);
            if (!spreadColor(seg, BKG, 64))
                segOff = false;
            ovrSz+=sz;
        }
        FastLED.show(stripBrightness);
        allOff = segOff;
    }
    return allOff;
}
