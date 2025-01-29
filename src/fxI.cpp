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

FxI1::FxI1() : LedEffect(fxi1Desc) {
    wallStart = 0;
    wallEnd = FRAME_SIZE;
    prevWallStart = wallStart;
    prevWallEnd = wallEnd;
    currentPos = 0;
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
    wallStart = random8(1, FRAME_SIZE/4);
    wallEnd = FRAME_SIZE - wallStart;
    prevWallStart = 0;
    prevWallEnd = FRAME_SIZE;
    currentPos = wallStart;
    segmentLength = random8(1, 11);
    fgColor = random8();
    bgColor = -random8();
    tpl(0, wallStart-1) = ColorFromPalette(targetPalette, bgColor, 7, LINEARBLEND);
    tpl(wallStart, prevWallEnd-1) = BKG;
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
        wallEnd = FRAME_SIZE - random8(1, FRAME_SIZE/4);
    } else {
        wallStart = random8(1, FRAME_SIZE/4);
    }
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

        // adjust the speed
        if (random8() < 96)
            speed = forward ? csub8(speed, 1, 20) : cadd8(speed, 3, 250);

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
                speed = random8(80, 160);
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

