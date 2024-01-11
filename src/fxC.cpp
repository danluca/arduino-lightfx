//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
/**
 * Category C of light effects
 *
 */

#include "fxC.h"

using namespace FxC;
using namespace colTheme;

//~ Effect description strings stored in flash
const char fxc1Desc[] PROGMEM = "FXC1: blend between two concurrent animations";
const char fxc2Desc[] PROGMEM = "FXC2: blur function";
const char fxc3Desc[] PROGMEM = "FXC3: Perlin Noise for moving up and down the strand";
const char fxc4Desc[] PROGMEM = "FxC4: lightnings";
const char fxc5Desc[] PROGMEM = "FxC5: matrix";
const char fxc6Desc[] PROGMEM = "FxC6: one sine";

void FxC::fxRegister() {
    static FxC1 fxC1;
    static FxC2 fxC2;
    static FxC3 fxC3;
    static FxC4 fxC4;
    static FxC5 fxC5;
    static FxC6 fxC6;
}

/**
 * aanimations
 * By: Can't recall where I found this. Maybe Stefan Petrick.
 * Modified by: Andrew Tuline
 *
 * Date: January, 2017
 * This sketch demonstrates how to blend between two animations running at the same time.
 */
FxC1::FxC1() : LedEffect(fxc1Desc), setA(frame(0, FRAME_SIZE-1)), setB(leds, FRAME_SIZE) {
}

void FxC1::setup() {
    LedEffect::setup();
    brightness = 176;
}

void FxC1::run() {
    animationA();
    animationB();
    CRGBSet others(leds, setB.size(), NUM_PIXELS);

    //combine all into setB (it is backed by the strip)
    uint8_t ratio = beatsin8(2);
    for (uint16_t x = 0; x < setB.size(); x++) {
        setB[x] = blend(setA[x], setB[x], ratio);
    }
    replicateSet(setB, others);

    FastLED.show(stripBrightness);
}

void FxC1::animationA() {
    for (uint16_t x = 0; x<setA.size(); x++) {
        uint8_t clrIndex = (millis() / 10) + (x * 12);    // speed, length
        if (clrIndex > 128) clrIndex = 0;
        setA[x] = ColorFromPalette(palette, clrIndex, dim8_raw(clrIndex << 1), LINEARBLEND);
    }
}

void FxC1::animationB() {
    for (uint16_t x = 0; x<setB.size(); x++) {
        uint8_t clrIndex = (millis() / 5) - (x * 12);    // speed, length
        if (clrIndex > 128) clrIndex = 0;
        setB[x] = ColorFromPalette(palette, 255-clrIndex, dim8_raw(clrIndex << 1), LINEARBLEND);
    }
}

bool FxC1::windDown() {
    return transEffect.offWipe(true);
}

uint8_t FxC1::selectionWeight() const {
    return 35;
}
//Fx C2
/**
 * blur
 * By: ????
 * Modified by: Andrew Tuline
 *
 * Date: July, 2015
 * Let's try the blur function. If you look carefully at the animation, sometimes there's a smooth gradient between each LED.
 * Other times, the difference between them is quite significant. Thanks for the blur.
 *
 */

FxC2::FxC2() : LedEffect(fxc2Desc) {}

//void FxC2::setup() {
//    LedEffect::setup();
//}

void FxC2::run() {
    uint8_t blurAmount = dim8_raw( beatsin8(3,64, 192) );       // A sinewave at 3 Hz with values ranging from 64 to 192.
    tpl.blur1d(blurAmount);                        // Apply some blurring to whatever's already on the strip, which will eventually go black.

    uint16_t  i = beatsin16(  9, 0, tpl.size()-1);
    uint16_t  j = beatsin16( 7, 0, tpl.size()-1);
    uint16_t  k = beatsin16(  5, 0, tpl.size()-1);

    // The color of each point shifts over time, each at a different speed.
    uint16_t ms = millis();
    leds[(i+j)/2] = paletteFactory.isHolidayLimitedHue() ? ColorFromPalette(palette, ms/29) : CHSV( ms / 29, 200, 255);
    leds[(j+k)/2] = paletteFactory.isHolidayLimitedHue() ? ColorFromPalette(palette, ms/41) : CHSV( ms / 41, 200, 255);
    leds[(k+i)/2] = paletteFactory.isHolidayLimitedHue() ? ColorFromPalette(palette, ms/73) : CHSV( ms / 73, 200, 255);
    leds[(k+i+j)/3] = paletteFactory.isHolidayLimitedHue() ? ColorFromPalette(palette, ms/53) : CHSV( ms / 53, 200, 255);

    replicateSet(tpl, others);
    FastLED.show(stripBrightness);
}

void FxC2::windDownPrep() {
    transEffect.prepare(SELECTOR_SPOTS);
}

uint8_t FxC2::selectionWeight() const {
    return 5;
}

//Fx C3
/**
 * inoise8_mover
 * By: Andrew Tuline
 * Date: February, 2017
 *
 * We've used sine waves and counting to move pixels around a strand. In this case, I'm using Perlin Noise to move a pixel up and down the strand.
 * The advantage here is that it provides random natural movement without requiring lots of fancy math by joe programmer.
 */
const uint32_t xscale = 8192;                                         // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
const uint32_t yscale = 7680;                                         // Wouldn't recommend changing this on the fly, or the animation will be really blocky.

FxC3::FxC3() : LedEffect(fxc3Desc) {}

void FxC3::setup() {
    LedEffect::setup();
    brightness = 255;
    targetPalette = paletteFactory.mainPalette();
    palette = paletteFactory.secondaryPalette();
    dist = random();
}

void FxC3::run() {
    EVERY_N_MILLISECONDS(35) {
        uint16_t locn = inoise16(xscale, dist+yscale) % 0xFFFF;           // Get a new pixel location from moving noise.
        uint16_t pixlen = map(locn, 0, 0xFFFF, 0, tpl.size());                     // Map that to the length of the strand.
        leds[pixlen] = ColorFromPalette(palette, pixlen, brightness, LINEARBLEND);   // Use that value for both the location as well as the palette index colour for the pixel.

        dist += beatsin16(10,128,8192);             // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
        tpl.fadeToBlackBy(4);

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
    EVERY_N_SECONDS(5) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }
    if (!paletteFactory.isHolidayLimitedHue()) {
        EVERY_N_SECONDS(25) {
            targetPalette = PaletteFactory::randomPalette();
        }
    }

}

JsonObject & FxC3::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["xscale"] = xscale;
    obj["yscale"] = yscale;
    return obj;
}

bool FxC3::windDown() {
    return transEffect.offSpots();
}

uint8_t FxC3::selectionWeight() const {
    return 4;
}

// Fx C4
FxC4::FxC4() : LedEffect(fxc4Desc) {}

void FxC4::setup() {
    LedEffect::setup();
    brightness = BRIGHTNESS;
    frequency = 10; // controls the interval between strikes (seconds)
    flashes = 7;   //the upper limit of flashes per strike
}

void FxC4::run() {
    EVERY_N_SECONDS_I(fxc4Timer, 1+random8(frequency)) {
        uint16_t start = random16(NUM_PIXELS - 8);                               // Determine starting location of flash
        uint16_t len = random16(4, NUM_PIXELS - start);                     // Determine length of flash (not to go beyond NUM_LEDS-1)
        uint8_t flashRound = random8(3, flashes);
        CRGBSet flash(leds, start, start+len);

        for (uint8_t flashCounter = 0; flashCounter < flashRound; flashCounter++) {
            // the brightness of the leader is scaled down by a factor of 5; return strokes are brighter than the leader
            uint8_t dimmer = flashCounter == 0 ? 5 : random8(1, 3);
            CRGB color = ColorFromPalette(palette, random8(), brightness / dimmer, LINEARBLEND);
            flash = color;
            FastLED.show(stripBrightness);                       // Show a section of LED's
            delay(random8(4, 10));                                     // each flash only lasts 4-10 milliseconds
            flash = BKG;
            FastLED.show(stripBrightness);

            if (flashCounter == 0) delay(250);                       // longer speed until next flash after the leader

            delay(50 + random8(100));                                   // shorter speed between strokes
        }
        fxc4Timer.setPeriod(1+random8(frequency));    // speed between strikes
    }
}

bool FxC4::windDown() {
    return true;
}

uint8_t FxC4::selectionWeight() const {
    return paletteFactory.getHoliday() == Halloween ? 20 : 2;
}

// Fx C5
FxC5::FxC5() : LedEffect(fxc5Desc) {}

void FxC5::setup() {
    LedEffect::setup();
    palette = paletteFactory.secondaryPalette();
    targetPalette = paletteFactory.mainPalette();
    saturation = 255;
    speed = 50;
    brightness = BRIGHTNESS;
}

void FxC5::run() {
    changeParams();

    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    EVERY_N_MILLISECONDS_I(c5Timer, speed) {
        matrix();
        c5Timer.setPeriod(speed);
        FastLED.show(stripBrightness);
    }

}

void FxC5::changeParams() {
    static uint8_t secSlot = 0;
    EVERY_N_SECONDS(5) {
        switch(secSlot) {
            case 0: speed=75; palIndex=95; bgClr=140; bgBri=4; hueRot=true; break;
            case 1: targetPalette = paletteFactory.mainPalette(); dirFwd=false; bgBri=0; hueRot=true; break;
            case 2: targetPalette = paletteFactory.secondaryPalette(); speed=45; palIndex=0; bgClr=50; bgBri=8; hueRot=false; dirFwd=true; break;
            case 3: if (!paletteFactory.isHolidayLimitedHue()) targetPalette = PaletteFactory::randomPalette(0, millis()); speed=95; bgBri = 12; bgClr=96; palIndex=random8(); break;
            case 4: palIndex=random8(); hueRot=true; break;
            default: break;
        }

        secSlot = inc(secSlot, 1, 5);   // Change '5' upper bound to a different value to change length of the loop.
    }
}

void FxC5::matrix() {
    if (hueRot) palIndex++;

    CRGB feed{};
    if (random8(90) > 80)
        feed = ColorFromPalette(palette, palIndex, brightness, LINEARBLEND);
    else if (paletteFactory.isHolidayLimitedHue())
        feed = ColorFromPalette(targetPalette, bgClr, bgBri);
    else
        feed = CHSV(bgClr, saturation, bgBri);
    if (dirFwd)
        shiftRight(tpl, feed);
    else
        shiftLeft(tpl, feed);
    replicateSet(tpl, others);
}

bool FxC5::windDown() {
    return transEffect.offWipe(false);
}

uint8_t FxC5::selectionWeight() const {
    return 20;
}

// Fx C6
FxC6::FxC6() : LedEffect(fxc6Desc) {}

void FxC6::setup() {
    LedEffect::setup();
    palette = paletteFactory.secondaryPalette();
    targetPalette = paletteFactory.mainPalette();
    speed = 8;
}

void FxC6::run() {
    static uint8_t secSlot = 0;

    EVERY_N_MILLISECONDS_I(c6Timer, delay) {
        one_sine_pal(millis()>>4);
        FastLED.show(stripBrightness);
        c6Timer.setPeriod(delay);
    }

    EVERY_N_SECONDS(1) {
        dirFwd = secSlot == 6;
        if (dirFwd)
            nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
        secSlot = inc(secSlot, 1, 7);
    }

    if (!paletteFactory.isHolidayLimitedHue()) {
        EVERY_N_SECONDS(10) {
            targetPalette = PaletteFactory::randomPalette(random8());
        }
    }
}

void FxC6::one_sine_pal(uint8_t colorIndex) {
    // This is the heart of this program. Sure is short.
    phase = dirFwd ? phase - speed : phase + speed;

    for (uint16_t k=0; k<tpl.size(); k++) {
        // For each of the LED's in the strand, set a brightness based on a wave as follows:
        uint8_t thisBright = qsubd(cubicwave8((k * allfreq) + phase), cutoff);         // qsub sets a minimum value called thiscutoff. If < thiscutoff, then bright = 0. Otherwise, bright = 128 (as defined in qsub)..
        tpl[k] = paletteFactory.isHolidayLimitedHue() ? ColorFromPalette(palette, bgclr, bgbright) : CHSV(bgclr, 255, bgbright);                                     // First set a background colour, but fully saturated.
        tpl[k] += ColorFromPalette(palette, colorIndex, thisBright, LINEARBLEND);    // Let's now add the foreground colour.
        colorIndex +=3;
    }
    replicateSet(tpl, others);
    bgclr++;
}

bool FxC6::windDown() {
    return transEffect.offWipe(true);
}

uint8_t FxC6::selectionWeight() const {
    return 20;
}
