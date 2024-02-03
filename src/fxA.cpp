//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
/**
 * Category A of light effects
 * 
 */
#include "fxA.h"

//~ Global variables definition for FxA
using namespace FxA;
using namespace colTheme;

//~ Effect description strings stored in flash
const char fxa1Desc[] PROGMEM = "FXA1: Multiple Tetris segments";
const char fxa2Desc[] PROGMEM = "FXA2: Randomly sized and spaced segments moving on entire strip";
const char fxa3Desc[] PROGMEM = "FXA3: Moving variable dot size back and forth";
const char fxa4Desc[] PROGMEM = "FXA4: pixel segments moving opposite directions";
const char fxa5Desc[] PROGMEM = "FXA5: Moving a color swath on top of another";

void FxA::fxRegister() {
    static FxA1 fxA1;
    static FxA2 fxA2;
    static FxA3 fxA3;
    static FxA4 fxA4;
    static FxA5 fxA5;
}

void FxA::resetStack() {
    szStack = 0;
    curPos = 0;
    mode = Chase;
}

///////////////////////////////////////
// Support Utilities
///////////////////////////////////////

uint16_t FxA::fxa_incStackSize(int16_t delta, uint16_t max) {
    szStack = capr(szStack + delta, 0, max);
    return szStack;
}

uint16_t FxA::fxa_stackAdjust(CRGBSet &set, uint16_t szStackSeg) {
    uint16_t minStack = szStackSeg << 1;
    if (szStack < minStack) {
        fxa_incStackSize(szStackSeg, set.size());
        return szStack;
    }
    if (colorIndex < 16) {
        shiftRight(set, BKG);
        fxa_incStackSize(-1, set.size());
    } else if (inr(colorIndex, 16, 32) || colorIndex == lastColorIndex) {
        fxa_incStackSize((int16_t) -szStackSeg, set.size());
    } else
        fxa_incStackSize(szStackSeg, set.size());
    return szStack;
}

///////////////////////////////////////
// Effect Definitions - setup and loop
///////////////////////////////////////
//FX A1
FxA1::FxA1() : LedEffect(fxa1Desc), dot(frame(0, FRAME_SIZE-1)) {
    dot.fill_solid(BKG);
}

void FxA1::setup() {
    LedEffect::setup();
    makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed + 50, brightness), LINEARBLEND), szSegment);
}

void FxA1::makeDot(CRGB color, uint16_t szDot) {
    for (uint16_t x = 0; x < szDot; x++) {
        dot[x] = color;
        dot[x] %= brightness;
    }
    dot[szDot - 1] = color;
    dot[szDot - 1] %= dimmed;
}

void FxA1::run() {
    if (mode == TurnOff) {
        if (transEffect.transition())
            resetStack();
        return;
    }
    EVERY_N_MILLISECONDS_I(a1Timer, speed) {
        uint16_t upLimit = tpl.size() - szStack;
        uint16_t movLimit = upLimit + szSegment - szStackSeg;
        shiftRight(tpl, curPos < szSegment ? dot[curPos] : BKG, (Viewport) upLimit);
        if (curPos == movLimit - 1)
            tpl[curPos - szSegment + 1] = dot[0];
        replicateSet(tpl, others);

        if (paletteFactory.getHoliday() == Halloween)
            FastLED.show(random8(32, stripBrightness)); //add a flicker
        else
            FastLED.show(stripBrightness);

        incr(curPos, 1, movLimit);
        if (curPos == 0) {
            fxa_stackAdjust(tpl, szStackSeg);
            if (szStack == tpl.size()) {
                mode = TurnOff;
                transEffect.prepare(transEffect.selector()+1);
            } else
                mode = Chase;
            //save the color
            lastColorIndex = colorIndex;
            colorIndex = random8();
            makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed + 60, brightness), LINEARBLEND),
                    szSegment);
            speed = random16(40, 81);
            a1Timer.setPeriod(speed);
        }
    }
}

JsonObject & FxA1::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["segmentSize"] = szSegment;
    return obj;
}

uint8_t FxA1::selectionWeight() const {
    return 3;
}

// FX A2
FxA2::FxA2() : LedEffect(fxa2Desc), dot(frame(0, FRAME_SIZE-1)) {
}

void FxA2::setup() {
    LedEffect::setup();
    makeDot();
}

void FxA2::makeDot() {
    uint8_t brdIndex = beatsin8(11);
    uint8_t brdBright = brdIndex % 2 ? BRIGHTNESS : dimmed;
    dot[szSegment - 1] = ColorFromPalette(palette, brdIndex, brdBright, LINEARBLEND);
    for (uint16_t x = 1; x < (szSegment - 1); x++) {
        dot[x] = ColorFromPalette(palette, colorIndex + random8(32), brightness, LINEARBLEND);
    }
    dot[0] = ColorFromPalette(palette, brdIndex, brdBright, LINEARBLEND);
}

void FxA2::run() {
    if (mode == TurnOff) {
        if (transEffect.transition())
            resetStack();
        return;
    }
    EVERY_N_MILLISECONDS_I(a2Timer, speed) {
        static CRGB feed = BKG;
        switch (movement) {
            case forward:
                shiftRight(tpl, feed);
                speed = beatsin8(1, 60, 150);
                break;
            case backward:
                shiftLeft(tpl, feed);
                speed = beatsin8(258, 50, 150);
                break;
            case pause:
                speed = 2000;
                movement = backward;
                break;
        }
        replicateSet(tpl, others);
        if (paletteFactory.getHoliday() == Halloween)
            FastLED.show(random8(32, stripBrightness)); //add a flicker
        else
            FastLED.show(stripBrightness);
        incr(curPos, 1, tpl.size());
        uint8_t ss = curPos % (szSegment + spacing);
        if (ss == 0) {
            //change segment, space sizes
            szSegment = beatsin8(11, 1, 7);
            spacing = beatsin8(8, 2, 11);
            colorIndex = beatsin8(10);
            makeDot();
        } else if (ss < szSegment)
            feed = movement == backward ? dot[ss] :
                    ColorFromPalette(palette, colorIndex, beatsin8(6, dimmed << 2, brightness, curPos), LINEARBLEND);
        else
            feed = BKG;

        if (curPos == 0)
            movement = forward;
        else if (curPos == (tpl.size() - tpl.size() / 4))
            movement = pause;
        a2Timer.setPeriod(speed);
    }

    EVERY_N_SECONDS(127) {
        if (countPixelsBrighter(&tpl) > 10) {
            mode = TurnOff;
            transEffect.prepare(transEffect.selector()+1);
        }
    }
}

JsonObject & FxA2::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["segmentSize"] = szSegment;
    return obj;
}

uint8_t FxA2::selectionWeight() const {
    return 10;
}

// Fx A3
FxA3::FxA3() : LedEffect(fxa3Desc), dot(frame(0, FRAME_SIZE-1)) {
}

void FxA3::setup() {
    LedEffect::setup();
    makeDot(ColorFromPalette(palette, colorIndex, brightness, LINEARBLEND), szSegment);
    bFwd = true;
    transEffect.prepare(random8());
}

void FxA3::makeDot(CRGB color, uint16_t szDot) {
    for (uint16_t x = 0; x < szDot; x++) {
        dot[x] = color;
        dot[x] %= brightness;
    }
    dot[szDot - 1] = color;
    dot[szDot - 1] %= dimmed;
}

void FxA3::run() {
    EVERY_N_MILLISECONDS_I(a3Timer, speed) {
        if (bFwd)
            shiftRight(tpl, curPos < szSegment ? dot[curPos] : BKG);
        else
            shiftLeft(tpl, curPos < szSegment ? dot[curPos] : BKG);
        replicateSet(tpl, others);
        if (paletteFactory.isHolidayLimitedHue())
            FastLED.show(random8(32, stripBrightness)); //add a flicker
        else
            FastLED.show(stripBrightness);

        incr(curPos, 1, tpl.size());
        if (curPos == 0) {
            bFwd = !bFwd;
            lastColorIndex = colorIndex;
            colorIndex = random8();
            speed = random8(40, 171);
            a3Timer.setPeriod(speed);
            szSegment = random8(2, 8);
            makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed << 2, brightness), LINEARBLEND),
                    szSegment);
        }
    }
}

JsonObject & FxA3::describeConfig(JsonArray &json) const {
    return LedEffect::describeConfig(json);
}

uint8_t FxA3::selectionWeight() const {
    return 20;
}

// FX A4
FxA4::FxA4() : LedEffect(fxa4Desc), dot(frame(0, FRAME_SIZE-1)), frL(frame(FRAME_SIZE, FRAME_SIZE*2-1)),
               frR(frame(FRAME_SIZE*2, FRAME_SIZE*3-1)), curBkg(BKG) {
}

void FxA4::setup() {
    LedEffect::setup();
    szSegment = 3;
    szStackSeg = 2;
    curBkg = BKG;
    brightness = 192;
    makeDot(ColorFromPalette(palette, colorIndex, brightness, LINEARBLEND), szSegment);
}

void FxA4::makeDot(CRGB color, uint16_t szDot) {
    CRGB c1 = color;
    CRGB c2 = ColorFromPalette(targetPalette, colorIndex, brightness, LINEARBLEND);
    dot(0, szDot).fill_gradient_RGB(c1, c2);
}

void FxA4::run() {
    if (mode == TurnOff) {
        if (transEffect.transition())
            resetStack();
        return;
    }

    EVERY_N_MILLISECONDS_I(a4Timer, speed) {
        curBkg = ColorFromPalette(palette, beatsin8(11), 3, LINEARBLEND);
        uint8_t ss = curPos % (szSegment + spacing);
        shiftRight(frR, ss < szSegment ? dot[ss] : curBkg);
        shiftLeft(frL,
                  curPos < szStackSeg ? ColorFromPalette(targetPalette, colorIndex, brightness, LINEARBLEND) : BKG);
        for (uint16_t x = 0; x < tpl.size(); x++) {
            tpl[x] = frR[x];
            tpl[x] += (frR[x] > curBkg) && frL[x] ? CRGB::White : frL[x];
        }
        replicateSet(tpl, others);
        if (paletteFactory.getHoliday() == Halloween)
            FastLED.show(random8(32, stripBrightness)); //add a flicker
        else
            FastLED.show(stripBrightness);

        incr(curPos, 1, FRAME_SIZE);
        if (curPos == 0) {
            colorIndex = random8();
            szSegment = beatsin8(21, 3, 7);
            spacing = beatsin8(15, szStackSeg + 4, 22);
            speed = random16(40, 171);
            a4Timer.setPeriod(speed);
            makeDot(ColorFromPalette(palette, colorIndex, brightness, LINEARBLEND), szSegment);
        }
    }

    EVERY_N_SECONDS(127) {
        if (countPixelsBrighter(&tpl) > 10) {
            mode = TurnOff;
            transEffect.prepare(transEffect.selector()+1);
        }
    }
}

JsonObject & FxA4::describeConfig(JsonArray &json) const {
    return LedEffect::describeConfig(json);
}

uint8_t FxA4::selectionWeight() const {
    return 20;
}

// Fx A5
FxA5::FxA5() : LedEffect(fxa5Desc), ovr(frame(0, FRAME_SIZE-1)) {
}

void FxA5::setup() {
    LedEffect::setup();
    lastColorIndex = 3;
    colorIndex = 7;
    makeFrame();
}

void FxA5::makeFrame() {
    uint8_t halfBright = brightness >> 1;
    uint8_t rndBright = random8(halfBright);
    bool mainPal = ovr[0].getParity();
    CRGBPalette16 &pal = mainPal ? palette : targetPalette;
    ovr.fill_solid(ColorFromPalette(pal, lastColorIndex, halfBright + rndBright, LINEARBLEND));
    CRGB newClr = ColorFromPalette(pal, colorIndex, halfBright + rndBright, LINEARBLEND);
    uint16_t seg = 5;
    for (uint16_t x = 0; x < seg; x++) {
        nblend(ovr[x], newClr, (seg-x-1)*50);
    }
    ovr[0].setParity(!mainPal);
}

void FxA5::run() {
    if (mode == TurnOff) {
        if (transEffect.transition())
            resetStack();
        return;
    }

    EVERY_N_MILLISECONDS_I(a5Timer, speed) {
        shiftRight(tpl, ovr[capu(curPos, ovr.size()-1)]);
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);

        incr(curPos, 1, tpl.size()+4);
        if (curPos == 0) {
            lastColorIndex = colorIndex;
            colorIndex = beat8(7);
            makeFrame();
            speed = 2000;
            a5Timer.setPeriod(speed);
        } else if (curPos == 1) {
            speed = random16(40, 131);
            a5Timer.setPeriod(speed);
        } else
            tpl(tpl.size()-1, capu(curPos+1, tpl.size()-1)).fadeToBlackBy(12);
    }

    EVERY_N_SECONDS(127) {
        if (countPixelsBrighter(&tpl) > 10) {
            mode = TurnOff;
            transEffect.prepare(transEffect.selector()+1);
        }
    }
}

JsonObject & FxA5::describeConfig(JsonArray &json) const {
    return LedEffect::describeConfig(json);
}

uint8_t FxA5::selectionWeight() const {
    return 20;
}
