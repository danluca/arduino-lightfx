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

/**
 * Register FxI effects
 */
void FxI::fxRegister() {
    static FxI1 fxI1;
}

//FXI1

FxI1::FxI1() : LedEffect(fxi1Desc), wallStart(0), wallEnd(FRAME_SIZE), currentPos(0), segmentLength(5), forward(true) {
    fgColor = 0;
    bgColor = 0;
    speed = 25;
}

void FxI1::setup() {
    LedEffect::setup();
    bgColor = random8();
    fgColor = random8();
    forward = true;
    reWall();
}

/**
 * Adjust walls at the end of a run
 */
void FxI1::reWall() {
    wallStart = random8(1, 16);
    wallEnd = FRAME_SIZE - wallStart;
    currentPos = wallStart;
    segmentLength = random8(1, 10);
    speed = random8(20, 250);
    fgColor += random8(0, 10);
    bgColor += random8(0, 16);
    const CRGB clr = ColorFromPalette(targetPalette, bgColor, 7, LINEARBLEND);
    tpl(0, wallStart-1) = clr;
    tpl(wallEnd, FRAME_SIZE-1) = clr;
    tpl(wallStart, wallEnd-1) = BKG;
}

void FxI1::run() {
    EVERY_N_MILLISECONDS_I(speed, 50) {
        // Fade the LED trail with a small dimming effect
        for (int i = wallStart; i < wallEnd; i++) {
            tpl[i].fadeToBlackBy(64);
        }

        // Draw the segment at the current position
        const CRGB clr = ColorFromPalette(palette, fgColor++, 255, LINEARBLEND);
        for (int i = 0; i < segmentLength; i++) {
            if ((currentPos + i) < wallEnd) {
                tpl[currentPos + i] = clr;
            }
        }

        if (random8() < 160)
            speed = forward ? csub8(speed, 3, 20) : cadd8(speed, 5, 250);

        // Update the position of the segment
        if (forward) {
            currentPos++;
            if ((currentPos + segmentLength) >= wallEnd) {
                forward = false; // Reverse direction at the wall
            }
        } else {
            currentPos--;
            if (currentPos <= wallStart) {
                forward = true; // Reverse direction at the wall
                reWall();
            }
        }

        // Display the updated LED state
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}


