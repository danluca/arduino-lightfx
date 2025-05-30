//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved
//
#include "fxA.h"
#include "transition.h"

//~ Global variables definition for FxA
using namespace FxA;
using namespace colTheme;

//~ Effect description strings stored in flash
constexpr auto fxa1Desc PROGMEM = "FXA1: Multiple Tetris segments";
constexpr auto fxa2Desc PROGMEM = "FXA2: Randomly sized and spaced segments moving on entire strip";
constexpr auto fxa3Desc PROGMEM = "FXA3: Moving variable dot size back and forth";
constexpr auto fxa4Desc PROGMEM = "FXA4: pixel segments moving opposite directions";
constexpr auto fxa5Desc PROGMEM = "FXA5: Moving a color swath on top of another";
constexpr auto fxa6Desc PROGMEM = "FXA6: Sleep Light";

uint16_t FxA::szStack = 0;

void FxA::fxRegister() {
    new FxA1();
    new FxA2();
    new FxA3();
    new FxA4();
    new FxA5();
    new SleepLight();
}

void FxA::resetStack() {
    szStack = 0;
    curPos = 0;
    mode = Chase;
}

///////////////////////////////////////
// Support Utilities
///////////////////////////////////////

uint16_t FxA::fxa_incStackSize(const int16_t delta, const uint16_t max) {
    szStack = capr(szStack + delta, 0, max);
    return szStack;
}

uint16_t FxA::fxa_stackAdjust(CRGBSet &set, const uint16_t szStackSeg) {
    if (const uint16_t minStack = szStackSeg << 1; szStack < minStack) {
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

void FxA1::makeDot(const CRGB color, const uint16_t szDot) const {
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
        const uint16_t upLimit = tpl.size() - szStack;
        const uint16_t movLimit = upLimit + szSegment - szStackSeg;
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

void FxA1::baseConfig(JsonObject &json) const {
    LedEffect::baseConfig(json);
    json["segmentSize"] = szSegment;
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

void FxA2::makeDot() const {
    const uint8_t brdIndex = beatsin8(11);
    const uint8_t brdBright = brdIndex % 2 ? BRIGHTNESS : dimmed;
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
        if (const uint8_t ss = curPos % (szSegment + spacing); ss == 0) {
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

void FxA2::baseConfig(JsonObject &json) const {
    LedEffect::baseConfig(json);
    json["segmentSize"] = szSegment;
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

void FxA3::makeDot(const CRGB color, const uint16_t szDot) const {
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

void FxA3::baseConfig(JsonObject &json) const {
    LedEffect::baseConfig(json);
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

void FxA4::makeDot(const CRGB color, const uint16_t szDot) {
    const CRGB c1 = color;
    const CRGB c2 = ColorFromPalette(targetPalette, colorIndex, brightness, LINEARBLEND);
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
        const uint8_t ss = curPos % (szSegment + spacing);
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

void FxA4::baseConfig(JsonObject &json) const {
    LedEffect::baseConfig(json);
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
    const uint8_t halfBright = brightness >> 1;
    const uint8_t rndBright = random8(halfBright);
    const bool mainPal = ovr[0].getParity();
    const CRGBPalette16 &pal = mainPal ? palette : targetPalette;
    ovr.fill_solid(ColorFromPalette(pal, lastColorIndex, halfBright + rndBright, LINEARBLEND));
    const CRGB newClr = ColorFromPalette(pal, colorIndex, halfBright + rndBright, LINEARBLEND);
    const uint16_t seg = 5;
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

void FxA5::baseConfig(JsonObject &json) const {
    LedEffect::baseConfig(json);
}

uint8_t FxA5::selectionWeight() const {
    return 20;
}

// SleepLight
SleepLight::SleepLight() : LedEffect(fxa6Desc), state(Fade), refPixel(&ledSet[0]) {
    for (int x = 5; x < ledSet.size(); x += 10) {
        slOffSegs.push_front(ledSet(x, x+4));
    }
}

uint8_t excludeActiveColors(const uint8_t hue) {
    constexpr uint8_t min = HUE_ORANGE-8;     //24       (~33°)
    constexpr uint8_t max = HUE_AQUA;         //128      (180°)
    return scale8(sin8(hue), (max-min)) + min;
}

void SleepLight::setup() {
    LedEffect::setup();
    FastLED.setTemperature(ColorTemperature::Tungsten40W);
    fill_solid(leds, NUM_PIXELS, colorBuf);
    timer=0;
    state = FadeColorTransition;
    hue = colorBuf.hue = excludeActiveColors(0);
    colorBuf.sat = 160;
    colorBuf.val = stripBrightness;
//    log_info(F("SleepLight setup: colorBuf=%r, hue=%d, sat=%d, val=%d"), (CRGB)colorBuf, colorBuf.hue, colorBuf.sat, colorBuf.val);
}

/**
 * Subtracts one number from another, preventing the number from dropping below a certain floor value.
 * The sanity check is accomplished via the qsub8 function.
 * @param val value to subtract from
 * @param sub value to subtract
 * @param floor min value not to go under
 * @return val-sub if greater than floor, floor otherwise
 */
uint8_t flrSub(const uint8_t val, const uint8_t sub, const uint8_t floor) {
    const uint8_t res = qsub8(val, sub);
    return res < floor ? floor : res;
}

void SleepLight::run() {
    if (state == Fade) {
        EVERY_N_SECONDS(5) {
            colorBuf.val = flrSub(colorBuf.val, 3, minBrightness);
            state = colorBuf.val > minBrightness ? FadeColorTransition : SleepTransition;
//            log_info(F("SleepLight parameters: state=%d, colorBuf=%r HSV=(%d,%d,%d), refPixel=%r"), state, (CRGB)colorBuf, colorBuf.hue, colorBuf.sat, colorBuf.val, *refPixel);
        }
        EVERY_N_SECONDS(3) {
            hue += random8(2, 19);
            colorBuf.hue = excludeActiveColors(hue);
            colorBuf.sat = map(colorBuf.val, minBrightness, brightness, 24, 160);
            state = colorBuf.val > minBrightness ? FadeColorTransition : SleepTransition;
//            log_info(F("SleepLight parameters: state=%d, colorBuf=%r HSV=(%d,%d,%d), refPixel=%r"), state, (CRGB)colorBuf, colorBuf.hue, colorBuf.sat, colorBuf.val, *refPixel);
        }
    }
    EVERY_N_MILLIS(125) {
        if (const SleepLightState oldState = step(); !(oldState == state && state == Sleep))
            FastLED.show(); //overall brightness is managed through color's value of HSV structure, which stabilizes at minBrightness, hence no need to scale with stripBrightness here
    }

}

SleepLight::SleepLightState SleepLight::step() {
    const SleepLightState oldState = state;
    switch (state) {
        case FadeColorTransition:
            if (rblend(*refPixel, (CRGB) colorBuf, 7))
                state = Fade;
            ledSet = *refPixel;
            break;
        case SleepTransition:
            timer = ++timer%12;
            if (timer == 0) {
                for (auto &seg : slOffSegs)
                    seg.fadeToBlackBy(1);
                if (slOffSegs.front()[0] == CRGB::Black)
                    state = Sleep;
            }
            break;
        default:
            break;
    }
//    if (oldState != state)
//        log_info(F("SleepLight state changed from %d to %d, colorBuf=%r, refPixel=%r"), oldState, state, (CRGB)colorBuf, *refPixel);
    return oldState;
}

uint8_t SleepLight::selectionWeight() const {
    return 0;   //we don't want this effect part of the random selection of entertaining light effects
}

void SleepLight::windDownPrep() {
    transEffect.prepare(SELECTOR_FADE);
}
