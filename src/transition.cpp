// Copyright (c) by Dan Luca. All rights reserved.
//

#include "transition.h"
#include "efx_setup.h"

// EffectTransition - we have 11 distinct off effects

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
    switch (prefFx ? (prefFx-1) : sel/2) {
        case 0: return offSpots();
        case 1: return offWipe(sel % 2);
        case 2: return offFade();
        case 3: return offSplit(sel % 2);
        case 4: return offRandomBars(sel % 2);
        case 5: return offHalfWipe(sel % 2);
        case 6: return offThermalEmberAsh(sel % 2);
        case 7: return offGravitationalImplosion(sel % 2);
        case 8: return offSparkleEvaporation(sel % 2);
        case 9: return offVenetianShutter(sel % 2);
        case 10: return offHarmonicWaveDamping(sel % 2);
        default:
            return offFade();
    }
}

void EffectTransition::prepare(const uint selector) {
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
    transStep = 0;

    if (randomBarSegs.empty())
        resetRandomBars();
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
        const uint16_t halfSize = NUM_PIXELS / 2;
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
        const uint16_t halfSize = NUM_PIXELS / 2;
        const uint16_t maxIndex = NUM_PIXELS - 1;
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

/**
 * Turns off entire strip with a thermal cooling and ember ash decay simulation
 * @param fastCool whether cooling happens rapidly or with a slower dying glow
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offThermalEmberAsh(bool fastCool) {
    bool allOff = false;
    EVERY_N_MILLIS(35) {
        bool anyOn = false;
        const uint8_t coolRate = fastCool ? 22 : 14;
        for (uint16_t i = 0; i < NUM_PIXELS; ++i) {
            CRGB &c = leds[i];
            if (c.r == 0 && c.g == 0 && c.b == 0) continue;

            // Thermal shift: cool down Blue and Green faster than Red to simulate dying coals
            c.b = qsub8(c.b, coolRate * 2);
            c.g = qsub8(c.g, coolRate);
            c.r = qsub8(c.r, coolRate / 2 + random8(1, 5));

            // Occasional micro-ember flare on still-glowing coals
            if (c.r > 30 && random8() < 20) {
                c.r = qadd8(c.r, random8(10, 35));
            }

            if (c.getLuma() < 6) {
                c = BKG;
            } else {
                anyOn = true;
            }
        }

        FastLED.show(stripBrightness);
        transStep++;

        if (!anyOn || transStep > 80) {
            fill_solid(leds, NUM_PIXELS, BKG);
            FastLED.show(stripBrightness);
            allOff = true;
        }
    }
    return allOff;
}

/**
 * Turns off entire strip by collapsing pixels inward to center or outward to strip boundaries with gravitational acceleration
 * @param outward whether collapse moves outward to ends (true) or inward to center (false)
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offGravitationalImplosion(bool outward) {
    bool allOff = false;
    EVERY_N_MILLIS(35) {
        const uint16_t halfSize = NUM_PIXELS / 2;
        // Gravity accelerates as distance closes
        const uint16_t step = 1 + (offPosIndex / 4);
        offPosIndex += step;

        if (offPosIndex >= halfSize) {
            fill_solid(leds, NUM_PIXELS, BKG);
            FastLED.show(stripBrightness);
            return true;
        }

        if (!outward) {
            // Collapse inward towards center
            for (uint16_t i = 0; i < offPosIndex; ++i) {
                leds[i] = BKG;
                if (NUM_PIXELS > 1 + i) {
                    leds[NUM_PIXELS - 1 - i] = BKG;
                }
            }
            // Suction compression and horizon fade near collapsing focal point
            const uint16_t leftFocal = halfSize - 1 - (offPosIndex >= halfSize ? halfSize - 1 : offPosIndex);
            const uint16_t rightFocal = halfSize + (offPosIndex >= halfSize ? halfSize - 1 : offPosIndex);
            if (leftFocal < NUM_PIXELS) leds[leftFocal].fadeToBlackBy(35);
            if (rightFocal < NUM_PIXELS) leds[rightFocal].fadeToBlackBy(35);
        } else {
            // Collapse outward towards ends (center splits and collapses to strip ends)
            for (uint16_t i = 0; i < offPosIndex; ++i) {
                if (halfSize >= 1 + i) leds[halfSize - 1 - i] = BKG;
                if (halfSize + i < NUM_PIXELS) leds[halfSize + i] = BKG;
            }
            leds[0].fadeToBlackBy(35);
            leds[NUM_PIXELS - 1].fadeToBlackBy(35);
        }

        FastLED.show(stripBrightness);
    }
    return allOff;
}

/**
 * Turns off entire strip by evaporating active pixels through energetic sparkle bursts and progressive decay
 * @param colorSparkle whether evaporation sparkles use palette hues or bright white flashes
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offSparkleEvaporation(bool colorSparkle) {
    bool allOff = false;
    EVERY_N_MILLIS(30) {
        // Base decay across whole strip
        CRGBSet strip(leds, NUM_PIXELS);
        strip.fadeToBlackBy(22);

        // Flash and boil off a random subset of active pixels
        const uint8_t burstCount = constrain(static_cast<uint8_t>(NUM_PIXELS / 18), static_cast<uint8_t>(2), static_cast<uint8_t>(32));
        for (uint8_t k = 0; k < burstCount; ++k) {
            const uint16_t idx = random16(NUM_PIXELS);
            if (leds[idx].getLuma() > 8) {
                leds[idx] = colorSparkle ? ColorFromPalette(palette, random8(), 255) : CRGB(255, 255, 255);
            }
        }

        transStep++;
        FastLED.show(stripBrightness);

        if (transStep > 45 || !isAnyLedOn(leds, NUM_PIXELS, BKG)) {
            fill_solid(leds, NUM_PIXELS, BKG);
            FastLED.show(stripBrightness);
            allOff = true;
        }
    }
    return allOff;
}

/**
 * Turns off entire strip using multi-iris venetian shutter blades closing synchronously
 * @param inward whether each iris blade closes inward from both edges (true) or wipes unidirectionally (false)
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offVenetianShutter(bool inward) {
    bool allOff = false;
    EVERY_N_MILLIS(40) {
        const uint8_t slats = NUM_PIXELS > 150 ? 16 : 8;
        const uint16_t slatWidth = (NUM_PIXELS + slats - 1) / slats;
        const uint16_t maxProgress = inward ? ((slatWidth + 1) / 2) : slatWidth;

        offPosIndex++;

        for (uint8_t s = 0; s < slats; ++s) {
            const uint16_t start = s * slatWidth;
            const uint16_t end = (s + 1 == slats) ? NUM_PIXELS : (start + slatWidth);
            if (start >= NUM_PIXELS) break;

            if (inward) {
                // Close from both edges of each shutter inward
                for (uint16_t p = 0; p < offPosIndex; ++p) {
                    if (start + p < end) leds[start + p] = BKG;
                    if (end > 1 + p && end - 1 - p >= start) leds[end - 1 - p] = BKG;
                }
                // Soft shutter blade edge
                if (start + offPosIndex < end) leds[start + offPosIndex].fadeToBlackBy(130);
                if (end > 1 + offPosIndex && end - 1 - offPosIndex >= start) leds[end - 1 - offPosIndex].fadeToBlackBy(130);
            } else {
                // Wipe across slat
                for (uint16_t p = 0; p < offPosIndex && (start + p < end); ++p) {
                    leds[start + p] = BKG;
                }
                if (start + offPosIndex < end) leds[start + offPosIndex].fadeToBlackBy(130);
            }
        }

        FastLED.show(stripBrightness);
        if (offPosIndex >= maxProgress) {
            fill_solid(leds, NUM_PIXELS, BKG);
            FastLED.show(stripBrightness);
            allOff = true;
        }
    }
    return allOff;
}

/**
 * Turns off entire strip through harmonic spatial standing/traveling wave damping
 * @param travelingWave whether wave nodes travel across the strip (true) or damp stationary (false)
 * @return true if all leds are off, false otherwise
 */
bool EffectTransition::offHarmonicWaveDamping(bool travelingWave) {
    bool allOff = false;
    EVERY_N_MILLIS(35) {
        transStep++;
        // Damping envelope decays from 255 down to 0
        const uint8_t damping = qsub8(255, transStep * 6);
        const uint8_t phase = travelingWave ? (transStep * 16) : 0;
        const uint16_t spatialFreq = (65536 / NUM_PIXELS) * 3; // 3 harmonic cycles

        for (uint16_t i = 0; i < NUM_PIXELS; ++i) {
            const uint8_t wave = sin8(static_cast<uint8_t>(i * spatialFreq / 256) + phase);
            const uint8_t localScale = scale8(wave, damping);
            leds[i].nscale8_video(localScale);
        }

        FastLED.show(stripBrightness);
        if (damping == 0 || transStep > 45) {
            fill_solid(leds, NUM_PIXELS, BKG);
            FastLED.show(stripBrightness);
            allOff = true;
        }
    }
    return allOff;
}
