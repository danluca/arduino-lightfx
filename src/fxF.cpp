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
}

void FxF1::loop() {
    EVERY_N_MILLISECONDS(speed) {
        uint8_t dotSize = 3;
        uint16_t w1 = beatsin16(12, 0, tpl.size()-dotSize-1);
        uint16_t w2 = beatsin16(12, 0, tpl.size()-dotSize-1, 0, 32768);
        CRGB clr1 = ColorFromPalette(palette, random8(), brightness, LINEARBLEND);
        CRGB clr2 = ColorFromPalette(targetPalette, random8(), brightness, LINEARBLEND);
        CRGBSet seg1 = tpl(w1, w1+dotSize);
        seg1 = clr1;
        seg1.blur1d(64);
        CRGBSet seg2 = tpl(w2, w2+dotSize);
        if (abs(w1-w2) <= dotSize)
            seg2.nblend(clr2, 128);
        else {
            seg2 = clr2;
            seg2.blur1d(64);
        }

        replicateSet(tpl, others);
        FastLED.show();
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
