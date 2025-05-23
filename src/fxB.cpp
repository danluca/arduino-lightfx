//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved
//
#include "fxB.h"
#include "transition.h"

//~ Global variables definition for FxB
using namespace FxB;
using namespace colTheme;

//~ Effect description strings stored in flash
constexpr auto fxb1Desc PROGMEM = "FXB1: rainbow";
constexpr auto fxb2Desc PROGMEM = "FXB2: rainbow with glitter";
constexpr auto fxb3Desc PROGMEM = "FXB3: confetti B";
constexpr auto fxb4Desc PROGMEM = "FXB4: sinelon";
constexpr auto fxb5Desc PROGMEM = "FXB5: juggle short segments";
constexpr auto fxb6Desc PROGMEM = "FXB6: bpm";
constexpr auto fxb7Desc PROGMEM = "FXB7: ease";
constexpr auto fxb8Desc PROGMEM = "FXB8: fadein";
constexpr auto fxb9Desc PROGMEM = "FXB9: juggle long segments";

uint16_t FxB::szStack = 0;

void FxB::fxRegister() {
    new FxB1();
    new FxB2();
    new FxB3();
    new FxB4();
    new FxB5();
    new FxB6();
    new FxB7();
    new FxB8();
    new FxB9();
}

//FXB1
FxB1::FxB1() : LedEffect(fxb1Desc) {}

void FxB1::setup() {
    LedEffect::setup();
    hue = 0;
    brightness = 148;
    transEffect.prepare(random8());
}

void FxB1::run() {
    EVERY_N_MILLISECONDS(60) {
        rainbow();
        FastLED.show(stripBrightness);
        hue += 2;
    }
}

void FxB::rainbow() {
    if (paletteFactory.isHolidayLimitedHue())
        tpl.fill_gradient_RGB(ColorFromPalette(palette, hue, brightness),
                              ColorFromPalette(palette, hue + 128, brightness),
                              ColorFromPalette(palette, 255 - hue, brightness));
    else {
        tpl.fill_rainbow(hue, 7);
        tpl.nscale8(brightness);
    }
    replicateSet(tpl, others);
}

void FxB1::baseConfig(JsonObject &json) const {
    LedEffect::baseConfig(json);
    json["brightness"] = brightness;
}

uint8_t FxB1::selectionWeight() const {
    return 15;
}

//FXB2
FxB2::FxB2() : LedEffect(fxb2Desc) {}

void FxB2::setup() {
    LedEffect::setup();
    hue = 0;
    brightness = 148;
    transEffect.prepare(random8());
}

void FxB2::run() {
    EVERY_N_MILLISECONDS(60) {
        rainbowWithGlitter();
        FastLED.show(stripBrightness);
        hue += 2;
    }
}

/**
 * Built-in FastLED rainbow, plus some random sparkly glitter.
 */
void FxB::rainbowWithGlitter() {
    rainbow();
    addGlitter(80);
}

void FxB::addGlitter(const fract8 chanceOfGlitter) {
    if (random8() < chanceOfGlitter) {
        leds[random16(NUM_PIXELS)] += CRGB::White;
    }
}

uint8_t FxB2::selectionWeight() const {
    return 40;
}

//FXB3
FxB3::FxB3() : LedEffect(fxb3Desc) {}

void FxB3::setup() {
    LedEffect::setup();
    hue = 0;
    brightness = 148;
    transEffect.prepare(random8());
}

void FxB3::run() {
    if (mode == TurnOff) {
        if (transEffect.transition())
            mode = Chase;
        else
            return;
    }

    EVERY_N_MILLISECONDS(50) {
        fxb_confetti();
    }
    EVERY_N_SECONDS(133) {
        if (countPixelsBrighter(&tpl) > 10)
            mode = TurnOff;
    }
}

void FxB::fxb_confetti() {
    // Random colored speckles that blink in and fade smoothly.
    tpl.fadeToBlackBy(10);
    const uint16_t pos = random16(tpl.size());
    if (paletteFactory.isHolidayLimitedHue())
        tpl[pos] += ColorFromPalette(palette, hue + random8(64));
    else
        tpl[pos] += CHSV(hue + random8(64), 200, 255);
    replicateSet(tpl, others);
    FastLED.show(stripBrightness);
    hue += 2;
}

void FxB3::baseConfig(JsonObject &json) const {
    LedEffect::baseConfig(json);
    json["brightness"] = brightness;
}

uint8_t FxB3::selectionWeight() const {
    return 24;
}

//FXB4
FxB4::FxB4() : LedEffect(fxb4Desc) {}

void FxB4::setup() {
    LedEffect::setup();
    hue = 0;
    brightness = 192;
    transEffect.prepare(random8());
}

void FxB4::run() {
    EVERY_N_MILLISECONDS(60) {
        sinelon();
        FastLED.show(stripBrightness);
        hue += 3;
    }
}

void FxB::sinelon() {
    // A colored dot sweeping back and forth, with fading trails.
    tpl.fadeToBlackBy(20);
    const uint16_t pos = beatsin16(14, 0, tpl.size() - 1);
    if (paletteFactory.isHolidayLimitedHue())
        tpl[pos] += ColorFromPalette(palette, hue, brightness);
    else
        tpl[pos] += CHSV(hue, 255, brightness);
    replicateSet(tpl, others);
}

void FxB4::baseConfig(JsonObject &json) const {
    LedEffect::baseConfig(json);
    json["brightness"] = brightness;
}

uint8_t FxB4::selectionWeight() const {
    return 20;
}

//FXB5
FxB5::FxB5() : LedEffect(fxb5Desc) {}

void FxB5::setup() {
    LedEffect::setup();
    brightness = BRIGHTNESS;
    transEffect.prepare(random8());
}

void FxB5::run() {
    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    EVERY_N_MILLIS(50) {
        juggle_short();
    }
}

/**
 * See juggle effect in the demoReel100_button example of FastLED-Demos repository
 * Eight colored dots, weaving in and out of sync with each other.
 */
void FxB::juggle_short() {
    constexpr uint16_t segSize = 8;
    tpl.fadeToBlackBy(20);

    for (uint16_t i = 0; i < segSize; i++) {
        // leds[beatsin16(i + 7, 0, NUM_PIXELS - 1)] |= CHSV(dothue, 200, 255);
        // note the |= operator may lead to colors outside the palette - for limited hues palettes (like Halloween) this may not be ideal
        const uint16_t pos = beatsin16(i + 7, 0, tpl.size() - 1);
        tpl[pos] |= ColorFromPalette(palette, hue, brightness, LINEARBLEND);
        curPos = pos;
        hue += 32;
    }
    replicateSet(tpl, others);
    FastLED.show(stripBrightness);
}

void FxB5::baseConfig(JsonObject &json) const {
    LedEffect::baseConfig(json);
    json["brightness"] = brightness;
}

uint8_t FxB5::selectionWeight() const {
    return 30;
}

//FXB6
FxB6::FxB6() : LedEffect(fxb6Desc) {}

void FxB6::setup() {
    LedEffect::setup();
    hue = 0;
    brightness = 148;
    transEffect.prepare(random8());
}

void FxB6::run() {
    EVERY_N_MILLISECONDS(50) {
        bpm();
    }
}

void FxB::bpm() {
    // Colored stripes pulsing at a defined Beats-Per-Minute.
    const uint8_t BeatsPerMinute = beatsin8(5, 42, 47);
    const uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);

    for (uint16_t i = 0; i < tpl.size(); i++) {
        leds[i] = ColorFromPalette(palette, hue + i, beat - hue + (i * 3));
    }
    replicateSet(tpl, others);
    hue += 8;  // slowly cycle the "base color" through the rainbow
    FastLED.show(stripBrightness);
}

uint8_t FxB6::selectionWeight() const {
    return 20;
}

//FXB7
FxB7::FxB7() : LedEffect(fxb7Desc) {}

void FxB7::setup() {
    LedEffect::setup();
    hue = 0;
    brightness = 148;
    transEffect.prepare(random8());
}

void FxB7::run() {
    EVERY_N_MILLISECONDS(75) {
        ease();
    }
}

void FxB::ease() {
    static uint16_t easeInVal = 0;

    const uint16_t easeOutVal = ease16InOutQuad(easeInVal);                     // Start with easeInVal at 0 and then go to 255 for the full easing.
    easeInVal += 811;         //completes a full 65536 cycle in about 6 seconds, given 75ms execution cadence

    const uint16_t lerpVal = lerp16by16(0, tpl.size() - 1, easeOutVal);                // Map it to the number of LED's you have.

    if (lerpVal != szStack) {
        if (lerpVal > curPos)
            tpl(curPos, lerpVal) = ColorFromPalette(palette, hue + easeInVal / 4, max(40, (uint8_t) easeOutVal));
        else
            tpl.fadeToBlackBy(49);
        curPos = lerpVal;
    }
    szStack = lerpVal;
    hue += 2;
    tpl.fadeToBlackBy(24);                          // 8 bit, 1 = slow fade, 255 = fast fade
    replicateSet(tpl, others);
    FastLED.show(stripBrightness);

}

void FxB7::baseConfig(JsonObject &json) const {
    LedEffect::baseConfig(json);
    json["brightness"] = brightness;
}

uint8_t FxB7::selectionWeight() const {
    return 10;
}

//FXB8
FxB8::FxB8() : LedEffect(fxb8Desc) {}

void FxB8::setup() {
    LedEffect::setup();
    hue = 0;
    brightness = 148;
    transEffect.prepare(random8());
}

void FxB8::run() {
    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    EVERY_N_MILLISECONDS(60) {
        fadein();
    }

    if (!paletteFactory.isHolidayLimitedHue()) {
        EVERY_N_SECONDS(10) {
            targetPalette = PaletteFactory::randomPalette(random8());
        }
    }
}

void FxB::fadein() {
    hueDiff = random16_get_seed();
    random16_set_seed(535);                                                           // The randomizer needs to be re-set each time through the loop in order for the 'random' numbers to be the same each time through.

    for (uint16_t i = 0; i < tpl.size(); i++) {
        const uint8_t fader = sin8(millis() / random8(10, 20));                                  // The random number for each 'i' will be the same every time.
        tpl[i] = ColorFromPalette(palette, i * 20, fader, LINEARBLEND);       // Now, let's run it through the palette lookup.
    }
    replicateSet(tpl, others);
    FastLED.show(stripBrightness);

    random16_set_seed(hueDiff);                                                      // Re-randomizing the random number seed for other routines.
}

void FxB8::baseConfig(JsonObject &json) const {
    LedEffect::baseConfig(json);
    json["brightness"] = brightness;
}

uint8_t FxB8::selectionWeight() const {
    return 15;
}

// FxB9
void FxB9::setup() {
    LedEffect::setup();
    //brightness = BRIGHTNESS;
    fade = 2;   // How long should the trails be. Very low value = longer trails.
    hueDiff = 16;   // Incremental change in hue between each dot.
    hue = 0;    // Starting hue.
    dotBpm = 5; // Higher = faster movement.
    transEffect.prepare(random8());
}

void FxB9::run() {
    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    juggle_long();
}

/**
 * See juggle_pal - Originally by: Mark Kriegsman; Modified By: Andrew Tuline
 * Date: February, 2015
 *
 * Juggle just moves some balls back and forth. A single ball could be a Cylon effect. I've added several variables to this simple routine.
 */
void FxB::juggle_long() {
    // Routine specific variables
    static uint8_t numDots = 4;                                     // Number of dots in use.
    static uint8_t secSlot = 0;

    EVERY_N_SECONDS(10) {
        switch (secSlot) {
            case 0: numDots = 1; dotBpm = 20; hueDiff = 16; fade = 17; hue = 0; break;
            case 1: numDots = 4; dotBpm = 10; hueDiff = 16; fade = 23; hue = 128; break;
            case 2: numDots = 8; dotBpm = 3; hueDiff = 2; fade = 29; hue = random8(); break;
            default:
                break;
        }
        secSlot = inc(secSlot, 1, 3);   //change the modulo value for changing the duration of the loop
    }
    EVERY_N_MILLIS(60) {
        uint8_t curHue = hue;                                           // Reset the hue values.
        tpl.fadeToBlackBy(fade);

        for (uint16_t i = 0; i < numDots; i++) {
            //  note the += operator may lead to colors outside the palette (less evident than |= operator) - for limited hues palettes (like Halloween) this may not be ideal
            const uint16_t pos = beatsin16(dotBpm + i + numDots, 0, tpl.size() - 1);
            tpl[pos] += ColorFromPalette(palette, curHue, brightness, LINEARBLEND);
            curHue += hueDiff;
        }
        hue += numDots * 2;
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

FxB9::FxB9() : LedEffect(fxb9Desc) {}

uint8_t FxB9::selectionWeight() const {
    return 14;
}
