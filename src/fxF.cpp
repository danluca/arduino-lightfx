/**
 * Category F of light effects
 *
 */
#include "fxF.h"

using namespace FxF;

void FxF::fxRegister() {
    static FxF1 fxF1;
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
        FastLED.show();
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
