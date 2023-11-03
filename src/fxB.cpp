/**
 * Category B of light effects
 *
 */
#include "fxB.h"

//~ Global variables definition for FxB
using namespace FxB;
using namespace colTheme;

//~ Effect description strings stored in flash
const char fxb1Desc[] PROGMEM = "FXB1: rainbow";
const char fxb2Desc[] PROGMEM = "FXB2: rainbow with glitter";
const char fxb3Desc[] PROGMEM = "FXB3: confetti B";
const char fxb4Desc[] PROGMEM = "FXB4: sinelon";
const char fxb5Desc[] PROGMEM = "FXB5: juggle short segments";
const char fxb6Desc[] PROGMEM = "FXB6: bpm";
const char fxb7Desc[] PROGMEM = "FXB7: ease";
const char fxb8Desc[] PROGMEM = "FXB8: fadein";
const char fxb9Desc[] PROGMEM = "FxB9: juggle long segments";

void FxB::fxRegister() {
    static FxB1 fxb1;
    static FxB2 fxB2;
    static FxB3 fxB3;
    static FxB4 fxB4;
    static FxB5 fxB5;
    static FxB6 fxB6;
    static FxB7 fxB7;
    static FxB8 fxB8;
    static FxB9 fxB9;
}

//FXB1
FxB1::FxB1() : LedEffect(fxb1Desc) {}

void FxB1::setup() {
    resetGlobals();
    hue = 0;
    brightness = 148;
}

void FxB1::loop() {
    EVERY_N_MILLISECONDS(60) {
        rainbow();
        FastLED.show(stripBrightness);
        hue+=2;
    }
}

void FxB::rainbow() {
    if (paletteFactory.isHolidayLimitedHue())
        tpl.fill_gradient_RGB(ColorFromPalette(palette, hue, brightness),
            ColorFromPalette(palette, hue+128, brightness),
            ColorFromPalette(palette, 255-hue, brightness));
    else
        tpl.fill_rainbow(hue, 7);
    replicateSet(tpl, others);
}


JsonObject & FxB1::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.getHoliday());
    return obj;
}

//FXB2
FxB2::FxB2() : LedEffect(fxb2Desc) {}

void FxB2::setup() {
    resetGlobals();
    hue = 0;
    brightness = 148;
}

void FxB2::loop() {
    EVERY_N_MILLISECONDS(60) {
        rainbowWithGlitter();
        FastLED.show(stripBrightness);
        hue+=2;
    }
}

/**
 * Built-in FastLED rainbow, plus some random sparkly glitter.
 */
void FxB::rainbowWithGlitter() {
    rainbow();
    nscale8(leds, NUM_PIXELS, brightness);
    addGlitter(80);
}

void FxB::addGlitter(fract8 chanceOfGlitter) {
    if (random8() < chanceOfGlitter) {
        leds[random16(NUM_PIXELS)] += CRGB::White;
    }
}

//FXB3
FxB3::FxB3() : LedEffect(fxb3Desc) {}

void FxB3::setup() {
    resetGlobals();
    hue = 0;
    brightness = 148;
}

void FxB3::loop() {
    if (mode == TurnOff) {
        if (turnOffWipe(true))
            mode = Chase;
    }

    EVERY_N_MILLISECONDS(50) {
        fxb_confetti();
    }
    EVERY_N_SECONDS(133) {
        if (countLedsOn(&tpl) > 10)
            mode = TurnOff;
    }
}

void FxB::fxb_confetti() {
    // Random colored speckles that blink in and fade smoothly.
    tpl.fadeToBlackBy(10);
    uint16_t pos = random16(tpl.size());
    if (paletteFactory.isHolidayLimitedHue())
        tpl[pos] += ColorFromPalette(palette, hue + random8(64));
    else
        tpl[pos] += CHSV(hue + random8(64), 200, 255);
    replicateSet(tpl, others);
    FastLED.show(stripBrightness);
    hue+=2;
}

JsonObject & FxB3::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.getHoliday());
    return obj;
}

//FXB4
FxB4::FxB4() : LedEffect(fxb4Desc) {}

void FxB4::setup() {
    resetGlobals();
    hue = 0;
    brightness = 192;
}

void FxB4::loop() {
  EVERY_N_MILLISECONDS(60) {
    sinelon();
    FastLED.show(stripBrightness);
    hue+=3;
  }
}

void FxB::sinelon() {
    // A colored dot sweeping back and forth, with fading trails.
    tpl.fadeToBlackBy(20);
    uint16_t pos = beatsin16(14, 0, tpl.size() - 1);
    if (paletteFactory.isHolidayLimitedHue())
        tpl[pos] += ColorFromPalette(palette, hue, brightness);
    else
        tpl[pos] += CHSV(hue, 255, brightness);
    replicateSet(tpl, others);
}

JsonObject & FxB4::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.getHoliday());
    return obj;
}

//FXB5
FxB5::FxB5() : LedEffect(fxb5Desc) {}

void FxB5::setup() {
    resetGlobals();
    brightness = BRIGHTNESS;
}

void FxB5::loop() {
    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    EVERY_N_MILLIS(50) {
        juggle_short();
        FastLED.show(stripBrightness);
    }
}

/**
 * See juggle effect in the demoReel100_button example of FastLED-Demos repository
 * Eight colored dots, weaving in and out of sync with each other.
 */
void FxB::juggle_short() {
    const uint16_t segSize = 8;
    tpl.fadeToBlackBy(20);
    byte dothue = 0;

    for (uint16_t i = 0; i < segSize; i++) {
        // leds[beatsin16(i + 7, 0, NUM_PIXELS - 1)] |= CHSV(dothue, 200, 255);
        // note the |= operator may lead to colors outside the palette - for limited hues palettes (like Halloween) this may not be ideal
        tpl[beatsin16(i + 7, 0, tpl.size() - 1)] |= ColorFromPalette(palette, dothue, brightness, LINEARBLEND);
        dothue += 32;
    }
    replicateSet(tpl, others);
}

JsonObject & FxB5::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.getHoliday());
    return obj;
}

//FXB6
FxB6::FxB6() : LedEffect(fxb6Desc) {}

void FxB6::setup() {
    resetGlobals();
    hue = 0;
    brightness = 148;
}

void FxB6::loop() {
    EVERY_N_MILLISECONDS(50) {
        bpm();
        hue += 2;  // slowly cycle the "base color" through the rainbow
        FastLED.show(stripBrightness);
    }
}

void FxB::bpm() {
    // Colored stripes pulsing at a defined Beats-Per-Minute.
    uint8_t BeatsPerMinute = beatsin8(5, 62, 67);
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);

    for (uint16_t i = 0; i < NUM_PIXELS; i++) {
        leds[i] = ColorFromPalette(palette, hue + (i * 2), beat - hue + (i * 10));
    }
}

//FXB7
FxB7::FxB7() : LedEffect(fxb7Desc) {}

void FxB7::setup() {
    resetGlobals();
    hue = 0;
    brightness = 148;
}

void FxB7::loop() {
    EVERY_N_MILLISECONDS(75) {
        ease();
        hue+=2;
        FastLED.show(stripBrightness);
    }

}

void FxB::ease() {
    static uint16_t easeInVal  = 0;

    uint16_t easeOutVal = ease16InOutQuad(easeInVal);                     // Start with easeInVal at 0 and then go to 255 for the full easing.
    easeInVal+=811;         //completes a full 65536 cycle in about 6 seconds, given 75ms execution cadence

    uint16_t lerpVal = lerp16by16(0, tpl.size(), easeOutVal);                // Map it to the number of LED's you have.

    if (lerpVal != szStack) {
        tpl[lerpVal] = ColorFromPalette(palette, hue + easeInVal / 4, max(40, (uint8_t) easeOutVal));
    }
    tpl.fadeToBlackBy(24);                          // 8 bit, 1 = slow fade, 255 = fast fade
    replicateSet(tpl, others);
    szStack = lerpVal;
}

JsonObject & FxB7::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.getHoliday());
    return obj;
}

//FXB8
FxB8::FxB8() : LedEffect(fxb8Desc) {}

void FxB8::setup() {
    resetGlobals();
    hue = 0;
    brightness = 148;
}

void FxB8::loop() {
    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    EVERY_N_MILLISECONDS(60) {
        fadein();
        FastLED.show(stripBrightness);
    }

    if (!paletteFactory.isHolidayLimitedHue()) {
        EVERY_N_SECONDS(10) {
            targetPalette = PaletteFactory::randomPalette(random8());
        }
    }
}

void FxB::fadein() {
    random16_set_seed(535);                                                           // The randomizer needs to be re-set each time through the loop in order for the 'random' numbers to be the same each time through.

    for (uint16_t i = 0; i<tpl.size(); i++) {
        uint8_t fader = sin8(millis()/random8(10,20));                                  // The random number for each 'i' will be the same every time.
        tpl[i] = ColorFromPalette(palette, i*20, fader, LINEARBLEND);       // Now, let's run it through the palette lookup.
    }
    replicateSet(tpl, others);

    random16_set_seed(millis() >> 5);                                                      // Re-randomizing the random number seed for other routines.
}

JsonObject & FxB8::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.getHoliday());
    return obj;
}

// FxB9
void FxB9::setup() {
    resetGlobals();
    //brightness = BRIGHTNESS;
    fade = 2;   // How long should the trails be. Very low value = longer trails.
    hueDiff = 16;   // Incremental change in hue between each dot.
    hue = 0;    // Starting hue.
    dotBpm = 5; // Higher = faster movement.
}

void FxB9::loop() {
    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    EVERY_N_MILLIS(60) {
        juggle_long();
        FastLED.show(stripBrightness);
    }
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
        switch(secSlot) {
            case 0: numDots = 1; dotBpm = 20; hueDiff = 16; fade = 2; hue = 0; break;
            case 1: numDots = 4; dotBpm = 10; hueDiff = 16; fade = 8; hue = 128; break;
            case 2: numDots = 8; dotBpm =  3; hueDiff =  0; fade = 8; hue=random8(); break;
            default: break;
        }
        secSlot = inc(secSlot, 1, 3);   //change the modulo value for changing the duration of the loop
    }

    uint8_t curHue = hue;                                           // Reset the hue values.
    tpl.fadeToBlackBy(fade);

    for(uint16_t i = 0; i < numDots; i++) {
        //  note the += operator may lead to colors outside the palette (less evident than |= operator) - for limited hues palettes (like Halloween) this may not be ideal
        tpl[beatsin16(dotBpm + i + numDots, 0, tpl.size() - 1)] += ColorFromPalette(palette, curHue , brightness, LINEARBLEND);
        curHue += hueDiff;
    }
    hue += numDots*2;
    replicateSet(tpl, others);
}

FxB9::FxB9() : LedEffect(fxb9Desc) {}
