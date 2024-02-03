//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
/**
 * Category H of light effects
 *
 */
#include "fxH.h"

using namespace FxH;
using namespace colTheme;

//~ Effect description strings stored in flash
const char fxh1Desc[] PROGMEM = "FXH1: Fire segments";
const char fxh2Desc[] PROGMEM = "FXH2: confetti H";
const char fxh3Desc[] PROGMEM = "FXH3: filling the strand with colours";

void FxH::fxRegister() {
    static FxH1 fxH1;
    static FxH2 fxH2;
    static FxH3 fxH3;
}

// Fire2012 with programmable Color Palette standard example, broken into multiple segments
// Based on Fire2012 by Mark Kriegsman, July 2012 as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
// Four different static color palettes are provided here, plus one dynamic one.
//
// The three static ones are:
//   1. the FastLED built-in HeatColors_p -- this is the default, and it looks
//      pretty much exactly like the original Fire2012.
//
//  To use any of the other palettes below, just "uncomment" the corresponding code and make the gPal non-constant
//
//   2. a gradient from black to red to yellow to white, which is
//      visually similar to the HeatColors_p, and helps to illustrate
//      what the 'heat colors' palette is actually doing,
//   3. a similar gradient, but in blue colors rather than red ones,
//      i.e. from black to blue to aqua to white, which results in
//      an "icy blue" fire effect,
//   4. a simplified three-step gradient, from black to red to white, just to show
//      that these gradients need not have four components; two or
//      three are possible, too, even if they don't look quite as nice for fire.
//
// The dynamic palette shows how you can change the basic 'hue' of the
// color palette every time through the loop, producing "rainbow fire".

FxH1::FxH1() : LedEffect(fxh1Desc), fires{tpl(0, FRAME_SIZE/2-1), tpl(FRAME_SIZE-1, FRAME_SIZE/2)} {
}

void FxH1::setup() {
    LedEffect::setup();
    brightness = 216;

    //Fire palette definition - for New Year get a blue fire
    switch (paletteFactory.getHoliday()) {
        case NewYear: palette = CRGBPalette16( CRGB::Black, CRGB::Blue, CRGB::Aqua,  CRGB::White); break;
        case Christmas: palette = CRGBPalette16(CRGB::Red, CRGB::White, CRGB::Green); break;
        default: palette = CRGBPalette16(CRGB::Black, CRGB::Red, CRGB::OrangeRed, CRGB::Yellow); break;
    }

    //clear the fires
    uint16_t maxSize = 0;
    for (auto & fire : fires) {
        fire.fill_solid(BKG);
        maxSize = max(maxSize, fire.size());
    }

    //initialize the heat map
    if (hMap.size() < maxSize) {
        for (uint16_t x = hMap.size(); x < maxSize; x++)
            hMap.push_back(BKG);
    } else if (hMap.size() > maxSize)
        hMap.erase(hMap.begin() + maxSize, hMap.end());

    // This first palette is the basic 'black body radiation' colors, which run from black to red to bright yellow to white.
    //gPal = HeatColors_p;

    // These are other ways to set up the color palette for the 'fire'.
    // First, a gradient from black to red to yellow to white -- similar to HeatColors_p
    //   gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Yellow, CRGB::White);

    // Second, this palette is like the heat colors, but blue/aqua instead of red/yellow
    //   gPal = CRGBPalette16( CRGB::Black, CRGB::Blue, CRGB::Aqua,  CRGB::White);

    // Third, here's a simpler, three-step gradient, from black to red to white
    //   gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::White);
}

void FxH1::run() {
    EVERY_N_MILLIS(1000 / FRAMES_PER_SECOND) {
        // Add entropy to random number generator; we use a lot of it.
        random16_add_entropy(random());

        // Fourth, the most sophisticated: this one sets up a new palette every
        // time through the loop, based on a hue that changes every time.
        // The palette is a gradient from black, to a dark color based on the hue,
        // to a light color based on the hue, to white.
        //
        //   static uint8_t hue = 0;
        //   hue++;
        //   CRGB darkcolor  = CHSV(hue,255,192); // pure hue, three-quarters brightness
        //   CRGB lightcolor = CHSV(hue,128,255); // half 'whitened', full brightness
        //   gPal = CRGBPalette16( CRGB::Black, darkcolor, lightcolor, CRGB::White);

        for (uint8_t x=0; x<numFires; x++)
            Fire2012WithPalette(x);  // run simulation frame 1, using palette colors

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);  // display this frame
    }
}

void FxH1::Fire2012WithPalette(uint8_t xFire) {
    //we only have 3 fires (numFires = 3) - abort if called for more than that
    if (xFire >= numFires)
        return;

    // Step 1.  Cool down every cell a little
    CRGBSet fire = fires[xFire];
    for (uint16_t i = 0; i < fire.size(); i++) {
        hMap[i][xFire] = qsub8(hMap[i][xFire], random8(0, ((COOLING * 10) / fire.size()) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (uint16_t k = fire.size() - 1; k >= 2; k--) {
        hMap[k][xFire] = (hMap[k - 1][xFire] + hMap[k - 2][xFire] + hMap[k - 2][xFire]) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if (random8() < SPARKING) {
        uint8_t y = random8(7);
        hMap[y][xFire] = qadd8(hMap[y][xFire], random8(160, 255));
    }

    // Step 4.  Map from heat cells to LED colors
    for (uint16_t j = 0; j < fire.size(); j++) {
        // Scale the heat value from 0-255 down to 0-240 for best results with color palettes.
        uint8_t colorIndex = scale8(hMap[j][xFire], 240);
        CRGB color = ColorFromPalette(palette, colorIndex);
        fire[j] = color;
        if (j > random8(5, 9))
            fire[j].nscale8(brightness);
    }
}

JsonObject & FxH1::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["flameBrightness"] = brightness;
    obj["numberOfFires"] = numFires;
    return obj;
}

void FxH1::windDownPrep() {
    transEffect.prepare(random8());
}

uint8_t FxH1::selectionWeight() const {
    return paletteFactory.getHoliday() == Halloween ? 64 : 32;
}

// FxH2
FxH2::FxH2() : LedEffect(fxh2Desc) {}

void FxH2::setup() {
    LedEffect::setup();
    speed = 40;
}

void FxH2::run() {
    updateParams();

    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    EVERY_N_MILLISECONDS(speed) {
        confetti_pal();
        FastLED.show(stripBrightness);
    }

}

void FxH2::confetti_pal() {
    // random colored speckles that blink in and fade smoothly
    tpl.fadeToBlackBy(fade);  // Low values = slower fade.
    uint16_t pos = random16(tpl.size());             // Pick an LED at random.
    tpl[pos] = ColorFromPalette(palette, hue + random16(hueDiff) / 4, brightness, LINEARBLEND);
    hue = hue + delta;  // It increments here.
    replicateSet(tpl, others);
}

void FxH2::updateParams() {
    static uint8_t secSlot = 0;
    EVERY_N_SECONDS(5) {
        switch (secSlot) {
            case 0: targetPalette = paletteFactory.mainPalette(); delta = 1; hue = 192; saturation = 255; fade = 2; hueDiff = 255; break;  // You can change values here, one at a time , or altogether.
            case 1: targetPalette = paletteFactory.secondaryPalette(); delta = 2; hue = 128; fade = 8; hueDiff = 64; break;
            case 2: if (!paletteFactory.isHolidayLimitedHue()) targetPalette = PaletteFactory::randomPalette(); delta = 1; hue = random16(255); fade = 1; hueDiff = 16; break;
            default: break;

        }
        secSlot = inc(secSlot, 1, 4);
    }
}

JsonObject & FxH2::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["brightness"] = brightness;
    obj["speed"] = speed;
    return obj;
}

void FxH2::windDownPrep() {
    transEffect.prepare(random8());
}

uint8_t FxH2::selectionWeight() const {
    return 24;
}

/**
 * fill_colours - TBD whether to keep, too close to rainbow march, etc.
 *
 * By: Andrew Tuline
 * Date: July, 2015
 *
 * This example provides ways of filling the strand with colours such as:
 * - fill_solid for a single colour across the entire strip
 * - fill_gradient for one or more colours across the section of, or the entire strip
 * - fill_rainbow for a whole enchalada of colours across sections of, or the entire the strip
 *
 * In order to keep the code simple, you will only see the last colour definition. Uncomment the one you want to see.
 *
 * References:
 * http://fastled.io/docs/3.1/group___colorutils.html
 * https://github.com/FastLED/FastLED/wiki/Pixel-reference#predefined-colors-list
 *
 */
// FxH3
FxH3::FxH3() : LedEffect(fxh3Desc) {}

void FxH3::setup() {
    LedEffect::setup();
    hueDiff = 15;
    hue = random8();
}

void FxH3::run() {
    // fill_rainbow section
    EVERY_N_MILLISECONDS(speed) {
        //below leds+1 is the same as &leds[1] - One pixel border at each end.
        if (paletteFactory.isHolidayLimitedHue())
            tpl(1, FRAME_SIZE-2).fill_gradient_RGB(ColorFromPalette(palette, hue, brightness),
              ColorFromPalette(palette, hue+128, brightness),
              ColorFromPalette(palette, 255-hue, brightness));
        else {
            tpl(1, FRAME_SIZE - 2).fill_rainbow(hue, hueDiff);
            tpl.nscale8(brightness);
        }
        hue += 3;
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }

}

JsonObject & FxH3::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["hueDiff"] = hueDiff;
    obj["speed"] = speed;
    return obj;
}

void FxH3::windDownPrep() {
    transEffect.prepare(random8());
}

uint8_t FxH3::selectionWeight() const {
    return 18;
}
