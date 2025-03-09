//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved
//
/**
 * Category I of light effects
 *
 */
#include "fxI.h"

using namespace FxI;
using namespace colTheme;

//~ Effect description strings stored in flash
constexpr auto fxi1Desc PROGMEM = "FXI1: Ping Pong";
constexpr auto fxi2Desc PROGMEM = "FXI2: Pacifica - gentle ocean waves";
constexpr auto fxi3Desc PROGMEM = "FXI3: Three overlay segments";

/**
 * Register FxI effects
 */
void FxI::fxRegister() {
    static FxI1 fxI1;
    static FxI2 fxI2;
}

//FXI1

FxI1::FxI1() : LedEffect(fxi1Desc) {
    wallStart = 0;
    wallEnd = FRAME_SIZE;
    prevWallStart = wallStart;
    prevWallEnd = wallEnd;
    currentPos = 0;
    midPointOfs = 0;
    segmentLength = 5;
    forward = true;
    fgColor = 0;
    bgColor = 0;
}

void FxI1::setup() {
    LedEffect::setup();
    bgColor = random8();
    fgColor = random8();
    forward = true;
    wallStart = random8(1, FRAME_SIZE / 4);
    wallEnd = FRAME_SIZE - wallStart;
    prevWallStart = 0;
    prevWallEnd = FRAME_SIZE;
    currentPos = wallStart;
    midPointOfs = 0;
    segmentLength = random8(1, 11);
    fgColor = random8();
    bgColor = -random8();
    tpl(0, wallStart - 1) = ColorFromPalette(targetPalette, bgColor, 7, LINEARBLEND);
    tpl(wallStart, prevWallEnd - 1) = BKG;
}

/**
 * Adjust walls at the end of a run
 */
void FxI1::reWall() {
    segmentLength = random8(1, 11);
    const uint8_t clrVar = random8(3, 19);
    bgColor += clrVar;
    fgColor += clrVar;
    if (forward) {
        wallEnd = FRAME_SIZE - random8(1, FRAME_SIZE / 4);
    } else {
        wallStart = random8(1, FRAME_SIZE / 4);
    }
    midPointOfs = random8(0, (wallEnd - wallStart) / 4) - (wallEnd - wallStart) / 8;
}

static void updateWall(uint16_t &prevWall, const uint16_t wallLimit, const CRGB color, const CRGB bg) {
    if (prevWall != wallLimit) {
        if (prevWall > wallLimit)
            tpl[prevWall--] = color;
        else
            tpl[prevWall++] = bg;
    }
}

static void blendWall(const uint16_t start, const uint16_t end, const CRGB color) {
    if (tpl[end] != color)
        tpl(start, end).nblend(color, 80);
}

void FxI1::run() {
    EVERY_N_MILLISECONDS_I(speed, 75) {
        //update the wall sizes and color
        const CRGB wallColor = ColorFromPalette(targetPalette, bgColor, 7, LINEARBLEND);
        if (forward) {
            updateWall(prevWallEnd, wallEnd, wallColor, BKG);
            if (prevWallEnd == wallEnd)
                blendWall(wallEnd, FRAME_SIZE - 1, wallColor);
        } else {
            updateWall(prevWallStart, wallStart, BKG, wallColor);
            if (prevWallStart == wallStart)
                blendWall(0, wallStart - 1, wallColor);
        }

        // Fade the LED trail with a small dimming effect
        for (int i = wallStart; i < wallEnd; i++) {
            tpl[i].fadeToBlackBy(80);
        }

        // Draw the segment at the current position
        const CRGB clr = ColorFromPalette(palette, fgColor++, 255, LINEARBLEND);
        for (int i = 0; i < segmentLength; i++) {
            if ((currentPos + i) < wallEnd) {
                tpl[currentPos + i] = clr;
            }
        }

        // adjust the speed - increase in first half, decrease in second half
        const uint16_t midPoint = (wallEnd - wallStart) / 2;
        if (forward) {
            if (currentPos < midPoint + midPointOfs)
                speed = csub8(speed, 1, 20);
            else
                speed = cadd8(speed, 3, 250);
        } else {
            if (currentPos > midPoint - midPointOfs)
                speed = csub8(speed, 1, 20);
            else
                speed = cadd8(speed, 3, 250);
        }

        // Update the position of the segment
        if (forward) {
            currentPos++;
            if ((currentPos + segmentLength) >= wallEnd) {
                forward = false; // Reverse direction at the wall
                // speed = csub8(speed, 10, 20);
                reWall();
            }
        } else {
            currentPos--;
            if (currentPos <= wallStart) {
                forward = true; // Reverse direction at the wall
                //speed = random8(80, 160);
                reWall();
            }
        }

        // Display the updated LED state
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

uint8_t FxI1::selectionWeight() const {
    return 7;
}

//FXI2 - Pacifica gentle ocean waves
FxI2::FxI2(): LedEffect(fxi2Desc) {
}

void FxI2::setup() {
    LedEffect::setup();
    sCIStart1 = sCIStart2 = sCIStart3 = sCIStart4 = 0;
    sLastMs = 0;
}

// These three custom blue-green color palettes were inspired by the colors found in
// the waters off the southern coast of California, https: //goo.gl/maps/QQgd97jjHesHZVxQ7
static const CRGBPalette16 pacifica_palette_1 PROGMEM = {
    0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
    0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50
};
static const CRGBPalette16 pacifica_palette_2 PROGMEM = {
    0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
    0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F
};
static const CRGBPalette16 pacifica_palette_3 PROGMEM = {
    0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33,
    0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF
};


void FxI2::pacifica_loop() {
    // Increment the four "color index start" counters, one for each wave layer.
    // Each is incremented at a different speed, and the speeds vary over time.
    const uint32_t ms = millis();
    const uint32_t deltaMs = ms - sLastMs;
    sLastMs = ms;
    const uint16_t speedFactor1 = beatsin16(3, 179, 269);
    const uint16_t speedFactor2 = beatsin16(4, 179, 269);
    const uint32_t deltaMs1 = (deltaMs * speedFactor1) / 256;
    const uint32_t deltaMs2 = (deltaMs * speedFactor2) / 256;
    const uint32_t deltaMs21 = (deltaMs1 + deltaMs2) / 2;
    sCIStart1 += (deltaMs1 * beatsin88(1011, 10, 13));
    sCIStart2 -= (deltaMs21 * beatsin88(777, 8, 11));
    sCIStart3 -= (deltaMs1 * beatsin88(501, 5, 7));
    sCIStart4 -= (deltaMs2 * beatsin88(257, 4, 6));

    // Clear out the LED array to a dim background blue-green
    tpl.fill_solid(CRGB(2, 6, 10));

    // Render each of four layers, with different scales and speeds, that vary over time
    pacifica_one_layer(pacifica_palette_1, sCIStart1, beatsin16(3, 11 * 256, 14 * 256),
        beatsin8(10, 70, 130), 0 - beat16(301));
    pacifica_one_layer(pacifica_palette_2, sCIStart2, beatsin16(4, 6 * 256, 9 * 256),
        beatsin8(17, 40, 80), beat16(401));
    pacifica_one_layer(pacifica_palette_3, sCIStart3, 6 * 256, beatsin8(9, 10, 38), 0 - beat16(503));
    pacifica_one_layer(pacifica_palette_3, sCIStart4, 5 * 256, beatsin8(8, 10, 28), beat16(601));

    // Add brighter 'whitecaps' where the waves lines up more
    pacifica_add_whitecaps();

    // Deepen the blues and greens a bit
    pacifica_deepen_colors();
}

// Add one layer of waves into the LED array
void FxI2::pacifica_one_layer(const CRGBPalette16 &p, const uint16_t ciStart, const uint16_t waveScale, const uint8_t bri, const uint16_t ioff) {
    uint16_t ci = ciStart;
    uint16_t waveAngle = ioff;
    const uint16_t waveScale_half = (waveScale / 2) + 20;
    for (uint16_t i = 0; i < tpl.size(); i++) {
        waveAngle += 250;
        const uint16_t s16 = sin16(waveAngle) + 32768;
        const uint16_t cs = scale16(s16, waveScale_half) + waveScale_half;
        ci += cs;
        const uint16_t sIndex16 = sin16(ci) + 32768;
        const uint8_t sIndex8 = scale16(sIndex16, 240);
        const CRGB c = ColorFromPalette(p, sIndex8, bri, LINEARBLEND);
        tpl[i] += c;
    }
}

// Add extra 'white' to areas where the four layers of light have lined up brightly
void FxI2::pacifica_add_whitecaps() {
    const uint8_t baseThreshold = beatsin8(9, 55, 65);
    uint8_t wave = beat8(7);

    for (uint16_t i = 0; i < tpl.size(); i++) {
        const uint8_t threshold = scale8(sin8(wave), 20) + baseThreshold;
        wave += 7;
        const uint8_t l = tpl[i].getAverageLight();
        if (l > threshold) {
            const uint8_t overage = l - threshold;
            const uint8_t overage2 = qadd8(overage, overage);
            tpl[i] += CRGB(overage, overage2, qadd8(overage2, overage2));
        }
    }
}

// Deepen the blues and greens
void FxI2::pacifica_deepen_colors() {
    for (uint16_t i = 0; i < tpl.size(); i++) {
        tpl[i].blue = scale8(tpl[i].blue, 145);
        tpl[i].green = scale8(tpl[i].green, 200);
        tpl[i] |= CRGB(2, 5, 7);
    }
}

/**
 * The code for this animation is more complicated than other examples, and while it is "ready to run",
 * and documented in general, it is probably not the best starting point for learning.  Nevertheless, it
 * does illustrate some useful techniques.
 * In this animation, there are four "layers" of waves of light.
 * Each layer moves independently, and each is scaled separately. All four wave layers are added together
 * on top of each other, and then another filter is applied that adds "whitecaps" of brightness where the
 * waves line up with each other more.  Finally, another pass is taken over the LED array to 'deepen' (dim)
 * the blues and greens.
 * The speed and scale and motion each layer varies slowly within independent hand-chosen ranges, which is
 * why the code has a lot of low-speed 'beatsin8' functions with a lot of oddly specific numeric ranges.
 *
 * Adapted from: https://gist.github.com/kriegsman/36a1e277f5b4084258d9af1eae29bac4
 * Name: Pacifica: gentle, blue-green ocean waves. For Dan.
 * Author: December 2019, Mark Kriegsman and Mary Corey March.
 */
void FxI2::run() {
    EVERY_N_MILLISECONDS_I(speed, 30) {
        pacifica_loop();
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

uint8_t FxI2::selectionWeight() const {
    return 9;
}

//FXI3 - three segments running at different speeds & directions in sawtooth style (allows motion to look contiguous in segments)
