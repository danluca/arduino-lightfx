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
const char fxh4Desc[] PROGMEM = "FXH4: TwinkleFox";
const char fxh5Desc[] PROGMEM = "FXH5: RainbowSparkle";

void FxH::fxRegister() {
    static FxH1 fxH1;
    static FxH2 fxH2;
    static FxH3 fxH3;
    static FxH4 fxH4;
    static FxH5 fxH5;
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

FxH1::FxH1() : LedEffect(fxh1Desc), fires{tpl(0, FRAME_SIZE / 2 - 1), tpl(FRAME_SIZE - 1, FRAME_SIZE / 2)} {
}

void FxH1::setup() {
    LedEffect::setup();
    brightness = 216;

    //Fire palette definition - for New Year get a blue fire
    switch (paletteFactory.getHoliday()) {
        case NewYear:
            palette = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::Aqua, CRGB::White);
            break;
        case Christmas:
            palette = CRGBPalette16(CRGB::Red, CRGB::White, CRGB::Green);
            break;
        default:
            palette = CRGBPalette16(CRGB::Black, CRGB::Red, CRGB::OrangeRed, CRGB::Yellow);
            break;
    }

    //clear the fires
    uint16_t maxSize = 0;
    for (auto &fire: fires) {
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

        for (uint8_t x = 0; x < numFires; x++)
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

JsonObject &FxH1::describeConfig(JsonArray &json) const {
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
            case 0:
                targetPalette = paletteFactory.mainPalette();
                delta = 1;
                hue = 192;
                saturation = 255;
                fade = 2;
                hueDiff = 255;
                break;  // You can change values here, one at a time , or altogether.
            case 1:
                targetPalette = paletteFactory.secondaryPalette();
                delta = 2;
                hue = 128;
                fade = 8;
                hueDiff = 64;
                break;
            case 2:
                if (!paletteFactory.isHolidayLimitedHue()) targetPalette = PaletteFactory::randomPalette();
                delta = 1;
                hue = random16(255);
                fade = 1;
                hueDiff = 16;
                break;
            default:
                break;

        }
        secSlot = inc(secSlot, 1, 4);
    }
}

JsonObject &FxH2::describeConfig(JsonArray &json) const {
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
            tpl(1, FRAME_SIZE - 2).fill_gradient_RGB(ColorFromPalette(palette, hue, brightness),
                                                     ColorFromPalette(palette, hue + 128, brightness),
                                                     ColorFromPalette(palette, 255 - hue, brightness));
        else {
            tpl(1, FRAME_SIZE - 2).fill_rainbow(hue, hueDiff);
            tpl.nscale8(brightness);
        }
        hue += 3;
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }

}

JsonObject &FxH3::describeConfig(JsonArray &json) const {
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

/**
 * TwinkleFOX: Twinkling 'holiday' lights that fade in and out.
 * This December 2015 implementation improves on the December 2014 version
 *  in several ways:
 *  - smoother fading, compatible with any colors and any palettes
 *  - easier control of twinkle speed and twinkle density
 *  - supports an optional 'background color'
 *  - takes even less RAM: zero RAM overhead per pixel
 *  - illustrates a couple of interesting techniques (uh oh...)
 *  The idea behind this (new) implementation is that there's one basic, repeating pattern that each pixel follows like a waveform:
 *  The brightness rises from 0..255 and then falls back down to 0. The brightness at any given point in time can be determined as
 *  as a function of time, for example:
 *    brightness = sine( time ); // a sine wave of brightness over time
 *    So the way this implementation works is that every pixel follows the exact same wave function over time.  In this particular case,
 * I chose a sawtooth triangle wave (triwave8) rather than a sine wave, but the idea is the same: brightness = triwave8( time ).
 * Of course, if all the pixels used the exact same wave form, and if they all used the exact same 'clock' for their 'time base', all
 * the pixels would brighten and dim at once -- which does not look like twinkling at all.
 * So to achieve random-looking twinkling, each pixel is given a slightly different 'clock' signal.  Some of the clocks run faster,
 * some run slower, and each 'clock' also has a random offset from zero. The net result is that the 'clocks' for all the pixels are always out
 * of sync from each other, producing a nice random distribution of twinkles.
 * The 'clock speed adjustment' and 'time offset' for each pixel are generated randomly.  One (normal) approach to implementing that
 * would be to randomly generate the clock parameters for each pixel at startup, and store them in some arrays.  However, that consumes
 * a great deal of precious RAM, and it turns out to be totally unnecessary!  If the random number generate is 'seeded' with the
 * same starting value every time, it will generate the same sequence of values every time.  So the clock adjustment parameters for each
 * pixel are 'stored' in a pseudo-random number generator!  The PRNG is reset, and then the first numbers out of it are the clock
 * adjustment parameters for the first pixel, the second numbers out of it are the parameters for the second pixel, and so on.
 * In this way, we can 'store' a stable sequence of thousands of random clock adjustment parameters in literally two bytes of RAM.
 * There's a little bit of fixed-point math involved in applying the clock speed adjustments, which are expressed in eighths.  Each pixel's
 * clock speed ranges from 8/8ths of the system clock (i.e. 1x) to 23/8ths of the system clock (i.e. nearly 3x).
 * On a basic Arduino Uno or Leonardo, this code can twinkle 300+ pixels smoothly at over 50 updates per seond.
 * Ref: https://github.com/FastLED/FastLED/blob/master/examples/TwinkleFox/TwinkleFox.ino
 * -Mark Kriegsman, December 2015
 */
//FxH4
FxH4::FxH4() : LedEffect(fxh4Desc) {
}

void FxH4::setup() {
    LedEffect::setup();
    targetPalette = PaletteFactory::selectNextPalette();
}

void FxH4::run() {
    EVERY_N_SECONDS(FxH4::secondsPerPalette) {
        targetPalette = PaletteFactory::selectNextPalette();
    }
    EVERY_N_MILLISECONDS(25) {
        nblendPaletteTowardPalette(palette, targetPalette, 12);
        drawTwinkles(tpl);
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

void FxH4::windDownPrep() {
    LedEffect::windDownPrep();
}

JsonObject &FxH4::describeConfig(JsonArray &json) const {
    return LedEffect::describeConfig(json);
}

uint8_t FxH4::selectionWeight() const {
    return 12;
}

//  This function loops over each pixel, calculates the adjusted 'clock' that this pixel should use, and calls
//  "CalculateOneTwinkle" on each pixel.  It then displays either the twinkle color of the background color,
//  whichever is brighter.
void FxH4::drawTwinkles(CRGBSet &set) {
    // "PRNG16" is the pseudorandom number generator. It MUST be reset to the same starting value each time
    // this function is called, so that the sequence of 'random' numbers that it generates is (paradoxically) stable.
    uint16_t PRNG16 = 11337;

    uint32_t clock32 = millis();

    // Set up the background color, "bg".
    CRGB bg;
    if (palette[0] == targetPalette[1]) {
        bg = palette[0];
        uint8_t bglight = bg.getAverageLight();
        if (bglight > 64) {
            bg.nscale8_video(16); // very bright, so scale to 1/16th
        } else if (bglight > 16) {
            bg.nscale8_video(64); // not that bright, so scale to 1/4th
        } else {
            bg.nscale8_video(86); // dim, scale to 1/3rd.
        }
    } else {
        bg = BKG; // just use the explicitly defined background color
    }

    uint8_t backgroundBrightness = bg.getAverageLight();

    for (auto &pixel: set) {
        PRNG16 = (uint16_t) (PRNG16 * 2053) + 1384; // next 'random' number
        uint16_t myclockoffset16 = PRNG16; // use that number as clock offset
        PRNG16 = (uint16_t) (PRNG16 * 2053) + 1384; // next 'random' number
        // use that number as clock speed adjustment factor (in 8ths, from 8/8ths to 23/8ths)
        uint8_t myspeedmultiplierQ5_3 = ((((PRNG16 & 0xFF) >> 4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
        uint32_t myclock30 = (uint32_t) ((clock32 * myspeedmultiplierQ5_3) >> 3) + myclockoffset16;
        uint8_t myunique8 = PRNG16 >> 8; // get 'salt' value for this pixel

        // We now have the adjusted 'clock' for this pixel, now we call
        // the function that computes what color the pixel should be based
        // on the "brightness = f( time )" idea.
        CRGB c = computeOneTwinkle(myclock30, myunique8);

        uint8_t cbright = c.getAverageLight();
        int16_t deltabright = cbright - backgroundBrightness;
        if (deltabright >= 32 || (!bg)) {
            // If the new pixel is significantly brighter than the background color, use the new color.
            pixel = c;
        } else if (deltabright > 0) {
            // If the new pixel is just slightly brighter than the background color, mix a blend of the new color and
            // the background color
            pixel = blend(bg, c, deltabright * 8);
        } else {
            // if the new pixel is not at all brighter than the background color, just use the background color.
            pixel = bg;
        }
    }
}

//  This function takes a time in pseudo-milliseconds, figures out brightness = f( time ), and also hue = f( time )
//  The 'low digits' of the millisecond time are used as input to the brightness wave function.
//  The 'high digits' are used to select a color, so that the color does not change over the course of the fade-in,
//  fade-out of one cycle of the brightness wave function. The 'high digits' are also used to determine whether this
//  pixel should light at all during this cycle, based on the TWINKLE_DENSITY.
CRGB FxH4::computeOneTwinkle(uint32_t ms, uint32_t salt) {
    uint16_t ticks = ms >> (8 - FxH4::twinkleSpeed);
    uint8_t fastcycle8 = ticks;
    uint16_t slowcycle16 = (ticks >> 8) + salt;
    slowcycle16 += sin8(slowcycle16);
    slowcycle16 = (slowcycle16 * 2053) + 1384;
    uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);

    uint8_t bright = 0;
    if (((slowcycle8 & 0x0E) / 2) < FxH4::twinkleDensity) {
        bright = attackDecayWave8(fastcycle8);
    }

    uint8_t hue = slowcycle8 - salt;
    CRGB c;
    if (bright > 0) {
        c = ColorFromPalette(palette, hue, bright, NOBLEND);
        coolLikeIncandescent(c, fastcycle8);
    } else {
        c = CRGB::Black;
    }
    return c;
}

// This function is like 'triwave8', which produces a symmetrical up-and-down triangle sawtooth waveform, except that
// this function produces a triangle wave with a faster attack and a slower decay:
//
//     / \
//    /     \
//   /         \
//  /             \
//

uint8_t FxH4::attackDecayWave8(uint8_t i) {
    if (i < 86) {
        return i * 3;
    } else {
        i -= 86;
        return 255 - (i + (i / 2));
    }
}

// This function takes a pixel, and if its in the 'fading down' part of the cycle, it adjusts the color a little bit
// like the way that incandescent bulbs fade toward 'red' as they dim.
void FxH4::coolLikeIncandescent(CRGB &c, uint8_t phase) {
    if (phase < 128) return;

    uint8_t cooling = (phase - 128) >> 4;
    c.g = qsub8(c.g, cooling);
    c.b = qsub8(c.b, cooling * 2);
}

//Ref: https://github.com/Electriangle/RainbowSparkle_Main/blob/main/Rainbow_Sparkle_Main.ino
//FxH5
FxH5::FxH5() : LedEffect(fxh5Desc), small(leds, 7), rest(leds, small.size(), NUM_PIXELS-1) {
}

void FxH5::setup() {
    LedEffect::setup();
}

void FxH5::run() {
    EVERY_N_MILLISECONDS(25) {
        int pixel = random(small.size());
        small[pixel].setRGB(red, green, blue);
        replicateSet(small, rest);
        FastLED.show();
        delay(colorTime);
        small[pixel].setRGB(0, 0, 0);
        electromagneticSpectrum(20);
    }
}

bool FxH5::windDown() {
    return LedEffect::windDown();
}

void FxH5::windDownPrep() {
    LedEffect::windDownPrep();
}

JsonObject &FxH5::describeConfig(JsonArray &json) const {
    return LedEffect::describeConfig(json);
}

uint8_t FxH5::selectionWeight() const {
    return 5;
}

void FxH5::electromagneticSpectrum(int transitionSpeed) {
    switch(colorTime) {
        case 1:
            green += transitionSpeed;
            blue -= transitionSpeed;
            if (green >= 255 or blue <= 0) {
                green = 255;
                blue = 0;
                colorTime = 2;
            }
            break;
        case 2:
            red += transitionSpeed;
            green -= transitionSpeed;
            if (red >= 255 or green <= 0) {
                red = 255;
                green = 0;
                colorTime = 3;
            }
            break;
        case 3:
            red -= transitionSpeed;
            blue += transitionSpeed;
            if (red <= 0 or blue >= 255) {
                red = 0;
                blue = 255;
                colorTime = 1;
            }
            break;
    }
}
