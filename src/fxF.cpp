/**
 * Category F of light effects
 *
 */
#include "fxF.h"

using namespace FxF;

void FxF::fxRegister() {
    static FxF1 fxF1;
    static FxF2 fxF2;
}

FxF1::FxF1() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxF1::setup() {
    resetGlobals();
    speed = 60;
    fade = 96;
    hue = random8();
    hueDiff = 8;
}

void FxF1::loop() {
    EVERY_N_MILLISECONDS(speed) {
        const uint8_t dotSize = 2;
        tpl.fadeToBlackBy(fade);

        uint16_t w1 = (beatsin16(12, 0, tpl.size()-dotSize-1) + beatsin16(24, 0, tpl.size()-dotSize-1))/2;
        uint16_t w2 = beatsin16(14, 0, tpl.size()-dotSize-1, 0, beat8(10)*128);

        CRGB clr1 = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
        CRGB clr2 = ColorFromPalette(targetPalette, hue, brightness, LINEARBLEND);

        CRGBSet seg1 = tpl(w1, w1+dotSize);
        seg1 = clr1;
        seg1.blur1d(64);
        CRGBSet seg2 = tpl(w2, w2+dotSize);
        seg2 |= clr2;

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
        hue += hueDiff;
    }
}

const char *FxF1::description() const {
    return "FxF1: beat wave";
}

const char *FxF1::name() const {
    return "FxF1";
}

void FxF1::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

// FxF2
FxF2::FxF2() : pattern(frame(0, FRAME_SIZE-1)) {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxF2::setup() {
    resetGlobals();
    hue = 2;
    hueDiff = 7;
    makePattern(hue);
}

void FxF2::loop() {
    // frame rate - 20fps
    EVERY_N_MILLISECONDS(50) {
        double dBreath = (exp(sin(millis()/2400.0*PI)) - 0.36787944)*108.0;//(exp(sin(millis()/2000.0*PI)) - 0.36787944)*108.0;       //(exp(sin(millis()/4000.0*PI)) - 0.36787944)*108.0;//(exp(sin(millis()/2000.0*PI)) - 0.36787944)*108.0;
        uint8_t breathLum = map(dBreath, 0, 255, 0, BRIGHTNESS);
        CRGB clr = ColorFromPalette(palette, hue, breathLum, LINEARBLEND);
        for (CRGBSet::iterator m=pattern.begin(), p=tpl.begin(), me=pattern.end(), pe=tpl.end(); m!=me && p!=pe; ++m, ++p) {
            if ((*m) == CRGB::White)
                (*p) = clr;
        }
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);

        if (breathLum < 2) {
            hue += hueDiff;
            makePattern(hue);
        }
    }
}

void FxF2::makePattern(uint8_t hue) {
    //clear the pattern, start over
    pattern = CRGB::Black;
    uint16_t s0 = random8(17);
    // pattern of XXX--XX-X-XX--XXX
    for (uint16_t i = 0; i < pattern.size(); i+=17) {
        pattern(i, i+2) = CRGB::White;
        pattern(i+5, i+6) = CRGB::White;
        pattern[i+8] = CRGB::White;
        pattern(i+10, i+11) = CRGB::White;
        pattern(i+14, i+16) = CRGB::White;
    }
    loopRight(pattern, (Viewport)0, s0);
}

const char *FxF2::description() const {
    return "FxF2: Halloween breathe with various color blends";
}

const char *FxF2::name() const {
    return "FxF2";
}

void FxF2::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

// FxF3
FxF3::FxF3() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxF3::setup() {
    resetGlobals();
}

void FxF3::loop() {

}

const char *FxF3::description() const {
    return "FxF3: Eye Blink";
}

const char *FxF3::name() const {
    return "FxF3";
}

void FxF3::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

