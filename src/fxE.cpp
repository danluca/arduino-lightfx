/**
 * Category E of light effects
 *
 */
#include "fxE.h"

using namespace FxE;

void FxE::fxRegister() {
    static FxE1 fxe1;
    static FxE2 fxE2;
    static FxE3 fxE3;
    static FxE4 fxE4;
}

/**
 * Display Template for FastLED
 * By: Andrew Tuline
 * Modified by: Andrew Tuline
 *
 * Date: July, 2015
 * This is a simple non-blocking FastLED display sequence template.
 *
 */
FxE1::FxE1() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxE1::setup() {
    resetGlobals();
    speed = 20;
    saturation = 255;
    brightness = 224;
}

void FxE1::loop() {
    updateParams();                                                 // Check the demo loop for changes to the variables.

    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    EVERY_N_MILLISECONDS_I(fxe1Timer, speed) {                           // FastLED based non-blocking speed to update/display the sequence.
        twinkle();
        FastLED.show();
        fxe1Timer.setPeriod(speed);
    }

    if (!paletteFactory.isHolidayLimitedHue()) {
        EVERY_N_SECONDS(10) {
            //static uint8_t baseC = random8();                         // You can use this as a baseline colour if you want similar hues in the next line.
            targetPalette = PaletteFactory::randomPalette();
        }
    }
}

const char *FxE1::description() const {
    return "FXE1: twinkle";
}


void FxE1::twinkle() {

  if (random8() < twinkrate) leds[random16(NUM_PIXELS)] += ColorFromPalette(palette, (randhue ? random8() : hue), brightness, currentBlending);
  fadeToBlackBy(leds, NUM_PIXELS, fade);
  
} // twinkle()

void FxE1::updateParams() {
    static uint8_t secSlot = 0;
    EVERY_N_SECONDS(5) {
        switch (secSlot) {
            case 0: speed = 25; randhue = true; saturation=255; fade=8; twinkrate=150; break;  // You can change values here, one at a time , or altogether.
            case 1: speed = 100; randhue = false; hue=random8(); fade=2; twinkrate=20; break;
            default: break;
        }
        secSlot = inc(secSlot, 1, 3);
    }
}

const char *FxE1::name() const {
    return "FXE1";
}

void FxE1::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["brightness"] = brightness;
}

// Fx E2
FxE2::FxE2() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxE2::setup() {
    resetGlobals();
    targetPalette = paletteFactory.mainPalette();
    palette = paletteFactory.secondaryPalette();
    saturation = 255;
    brightness = 224;
}

void FxE2::loop() {
    EVERY_N_MILLIS(100) {
        beatwave();
        FastLED.show();
    }

    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    if (!paletteFactory.isHolidayLimitedHue()) {
        EVERY_N_SECONDS(15) {
            targetPalette = PaletteFactory::randomPalette(0, millis());
        }
    }
}

const char *FxE2::description() const {
    return "FXE2: beatwave";
}

void FxE2::beatwave() {
    uint8_t wave1 = beatsin8(9, 0, 255);                        // That's the same as beatsin8(9);
    uint8_t wave2 = beatsin8(8, 0, 255);
    uint8_t wave3 = beatsin8(7, 0, 255);
    uint8_t wave4 = beatsin8(6, 0, 255);

    for (uint16_t i=0; i<tpl.size(); i++)
        tpl[i] = ColorFromPalette(palette, i + wave1 + wave2 + wave3 + wave4, brightness, currentBlending);
    replicateSet(tpl, others);
}

const char *FxE2::name() const {
    return "FXE2";
}

void FxE2::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

//Fx E3
void FxE3::setup() {
    resetGlobals();
    fade = dimmed;
    delta = 32;
    segStart = segEnd = 0;
    timerSlot = 0;
    cycles = 10;
    move = forward;
}

void FxE3::loop() {
        EVERY_N_MILLISECONDS(100) {
        if (timerSlot == 0) {
            uint8_t adv = random8(1, 5);
            uint16_t maxIndex = tpl.size() - 1;
            uint16_t newPos = capu(curPos+adv, maxIndex);
            switch (move) {
                case forward: segStart = curPos; segEnd = newPos; cycles = random8(8, 16); break;
                case backward: segStart = maxIndex - curPos; segEnd = maxIndex - newPos; cycles = 12; break;
            }
            fade = dimmed;
            colorIndex = beatsin8(7);
            curPos = newPos == maxIndex ? 0 : newPos + 1;
        }
        CRGBSet seg = tpl(segStart, segEnd);
        switch (move) {
            case forward: seg = ColorFromPalette(palette, colorIndex, fade, currentBlending); break;
            case backward: seg.fadeToBlackBy(fade); break;
        }

        fade = capu(fade + delta, brightness);
        incr(timerSlot, 1, cycles);

        if ((timerSlot == 0) && (curPos == 0)) {
            switch (move) {
                case forward: move = pause; timerSlot = 1; cycles = 30; break;
                case pause: move = seg ? backward : forward; break;
                case backward: move = pause; timerSlot = 1; cycles = 20; break;
            }
        }

        replicateSet(tpl, others);
        FastLED.show();
    }
}

const char *FxE3::description() const {
    return "FxE3: sawtooth";
}

const char *FxE3::name() const {
    return "FxE3";
}

void FxE3::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

FxE3::FxE3() {
    registryIndex = fxRegistry.registerEffect(this);
}

//Fx E4
void FxE4::setup() {
    resetGlobals();
    X = Xorig;
    Y = Yorig;
}

void FxE4::loop() {
    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    if (!paletteFactory.isHolidayLimitedHue()) {
        EVERY_N_SECONDS(30) {
            targetPalette = PaletteFactory::randomPalette(random8());
        }
    }

    EVERY_N_MILLISECONDS(50) {
        serendipitous();
        FastLED.show();
    }

}

const char *FxE4::description() const {
    return "FxE4: serendipitous";
}

const char *FxE4::name() const {
    return "FxE4";
}

void FxE4::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

FxE4::FxE4() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxE4::serendipitous() {
    //  Xn = X-(Y/2); Yn = Y+(Xn/2);
    //  Xn = X-Y/2;   Yn = Y+Xn/2;
    uint16_t Xn = X-(Y/2); uint16_t Yn = Y+(X/2.1); uint16_t Zn = X + Y*2.3;
    //    Xn = X-(Y/3); Yn = Y+(X/1.5);
    //  Xn = X-(2*Y); Yn = Y+(X/1.1);

    X = Xn;
    Y = Yn;

    index=(sin8(X)+cos8(Y))/2;
    CRGB newcolor = ColorFromPalette(palette, index, map(Zn, 0, 65535, dimmed*3, brightness), LINEARBLEND);

    nblend(tpl[map(X, 0, 65535, 0, tpl.size())], newcolor, 224);    // Try and smooth it out a bit. Higher # means less smoothing.
    tpl.fadeToBlackBy(16);                    // 8 bit, 1 = slow, 255 = fast
    replicateSet(tpl, others);
}
