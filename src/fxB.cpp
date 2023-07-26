/**
 * Category B of light effects
 *
 */
#include "fxB.h"

//~ Global variables definition
uint8_t gHue = 0;
CRGBPalette16 targetPalette;

void fxb_register() {
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

void fxb_setup() {
    FastLED.clear(true);
    FastLED.setBrightness(BRIGHTNESS);
    targetPalette = paletteFactory.mainPalette();
    palette = paletteFactory.secondaryPalette();
    gHue = 0;
    brightness = 148;
}

//FXB1
void FxB1::setup() {
    fxb_setup();
}

void FxB1::loop() {
    EVERY_N_MILLISECONDS(50) {
        rainbow();
        gHue+=2;
    }
    FastLED.show();
}

const char *FxB1::description() {
    return "FXB1: rainbow";
}

FxB1::FxB1() {
    registryIndex = fxRegistry.registerEffect(this);
}

const char *FxB1::name() {
    return "FXB1";
}

void FxB1::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());
}

//FXB2
FxB2::FxB2() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxB2::setup() {
    fxb_setup();
}

void FxB2::loop() {
    EVERY_N_MILLISECONDS(50) {
        rainbowWithGlitter();
        gHue+=2;
    }

    FastLED.show();
}

const char *FxB2::description() {
    return "FXB2: rainbowWithGlitter";
}

const char *FxB2::name() {
    return "FXB2";
}

void FxB2::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

//FXB3
FxB3::FxB3() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxB3::setup() {
    fxb_setup();
}

void FxB3::loop() {
    EVERY_N_MILLISECONDS(50) {
        fxb_confetti();
        gHue+=2;
    }

    FastLED.show();
}

const char *FxB3::description() {
    return "FXB3: fxb_confetti";
}

const char *FxB3::name() {
    return "FXB3";
}

void FxB3::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());
}

//FXB4
FxB4::FxB4() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxB4::setup() {
    fxb_setup();
}

void FxB4::loop() {
  EVERY_N_MILLISECONDS(50) {
    sinelon();
    gHue+=2;
  }

  FastLED.show();
}

const char *FxB4::description() {
    return "FXB4: sinelon";
}

const char *FxB4::name() {
    return "FXB4";
}

void FxB4::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());
}

//FXB5
FxB5::FxB5() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxB5::setup() {
    fxb_setup();
    brightness = BRIGHTNESS;
}

void FxB5::loop() {
    EVERY_N_SECONDS(5) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    juggle_short();
    FastLED.show();
}

const char *FxB5::description() {
    return "FXB5: juggle short segments";
}

const char *FxB5::name() {
    return "FXB5";
}

void FxB5::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());
}

//FXB6
FxB6::FxB6() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxB6::setup() {
    fxb_setup();
}

void FxB6::loop() {
    EVERY_N_MILLISECONDS(50) {
        bpm();
        gHue += 2;  // slowly cycle the "base color" through the rainbow
    }

    FastLED.show();
}

const char *FxB6::description() {
    return "FXB6: bpm";
}

const char *FxB6::name() {
    return "FXB6";
}

void FxB6::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

//FXB7
FxB7::FxB7() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxB7::setup() {
    fxb_setup();
}

void FxB7::loop() {
    EVERY_N_MILLISECONDS(50) {
        ease();
        gHue+=2;
    }

    FastLED.show();
}

const char *FxB7::description() {
    return "FXB7: ease";
}

const char *FxB7::name() {
    return "FXB7";
}

void FxB7::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());
}

//FXB8
FxB8::FxB8() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxB8::setup() {
    fxb_setup();
}

void FxB8::loop() {
    EVERY_N_MILLISECONDS(250) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    EVERY_N_MILLISECONDS(50) {
        fadein();
    }

    EVERY_N_SECONDS(10) {                                                        // Change the target palette to a random one every 10 seconds.
        targetPalette = PaletteFactory::randomPalette(random8());
    }

    FastLED.show();
}

const char *FxB8::description() {
    return "FXB8: fadein";
}

const char *FxB8::name() {
    return "FXB8";
}

void FxB8::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["brightness"] = brightness;
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());
}

void FxB9::setup() {
    fxb_setup();
    brightness = BRIGHTNESS;
    fade = 2;   // How long should the trails be. Very low value = longer trails.
    hueDiff = 16;   // Incremental change in hue between each dot.
    hue = 0;    // Starting hue.
    dotBpm = 5; // Higher = faster movement.
}

void FxB9::loop() {
    EVERY_N_SECONDS(5) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    juggle_long();
    FastLED.show();
}

const char *FxB9::description() {
    return "FxB9: juggle long segments";
}

const char *FxB9::name() {
    return "FxB9";
}

void FxB9::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

FxB9::FxB9() {
    registryIndex = fxRegistry.registerEffect(this);
}



void fadein() {

  random16_set_seed(535);                                                           // The randomizer needs to be re-set each time through the loop in order for the 'random' numbers to be the same each time through.

  for (int i = 0; i<NUM_PIXELS; i++) {
    uint8_t fader = sin8(millis()/random8(10,20));                                  // The random number for each 'i' will be the same every time.
    leds[i] = ColorFromPalette(palette, i*20, fader, LINEARBLEND);       // Now, let's run it through the palette lookup.
  }

  random16_set_seed(millis() >> 5);                                                      // Re-randomizing the random number seed for other routines.

} // fadein()


void rainbow() {

  fill_rainbow(leds, NUM_PIXELS, gHue, 7);  // FastLED's built-in rainbow generator.

}  // rainbow()



void rainbowWithGlitter() {

  rainbow();  // Built-in FastLED rainbow, plus some random sparkly glitter.
  nscale8(leds, NUM_PIXELS, brightness);
  addGlitter(80);

}  // rainbowWithGlitter()



void addGlitter(fract8 chanceOfGlitter) {

  if (random8() < chanceOfGlitter) {
    leds[random16(NUM_PIXELS)] += CRGB::White;
  }

}  // addGlitter()



void fxb_confetti() {  // Random colored speckles that blink in and fade smoothly.

  fadeToBlackBy(leds, NUM_PIXELS, 10);
  int pos = random16(NUM_PIXELS);
  leds[pos] += CHSV(gHue + random8(64), 200, 255);

}  // confetti()



void sinelon() {  // A colored dot sweeping back and forth, with fading trails.

  fadeToBlackBy(leds, NUM_PIXELS, 20);
  int pos = beatsin16(13, 0, NUM_PIXELS - 1);
  leds[pos] += CHSV(gHue, 255, 192);

}  // sinelon()



void bpm() {  // Colored stripes pulsing at a defined Beats-Per-Minute.

  uint8_t BeatsPerMinute = 62;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);

  for (int i = 0; i < NUM_PIXELS; i++) {  //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }

}  // bpm()

/**
 * See juggle effect in the demoReel100_button example of FastLED-Demos repository
 * Eight colored dots, weaving in and out of sync with each other.
 */
void juggle_short() {

  fadeToBlackBy(leds, NUM_PIXELS, 20);
  byte dothue = 0;

  for (int i = 0; i < 8; i++) {
//    leds[beatsin16(i + 7, 0, NUM_PIXELS - 1)] |= CHSV(dothue, 200, 255);
    leds[beatsin16(i + 7, 0, NUM_PIXELS - 1)] |= ColorFromPalette(palette, dothue, brightness, LINEARBLEND);
    dothue += 32;
  }

}  // juggle()

/**
 * See juggle_pal - Originally by: Mark Kriegsman; Modified By: Andrew Tuline
 * Date: February, 2015
 *
 * Juggle just moves some balls back and forth. A single ball could be a Cylon effect. I've added several variables to this simple routine.
 */
void juggle_long() {
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
    fadeToBlackBy(leds, NUM_PIXELS, fade);

    for(int i = 0; i < numDots; i++) {
        leds[beatsin16(dotBpm + i + numDots, 0, NUM_PIXELS - 1)] += ColorFromPalette(palette, curHue , brightness, LINEARBLEND);    // Munge the values and pick a colour from the palette
        curHue += hueDiff;
    }
}   //juggle_pal()

void ease() {
  static uint8_t easeOutVal = 0;
  static uint8_t easeInVal  = 0;
  static uint8_t lerpVal    = 0;

  easeOutVal = ease8InOutQuad(easeInVal);                     // Start with easeInVal at 0 and then go to 255 for the full easing.
  easeInVal++;

  lerpVal = lerp8by8(0, NUM_PIXELS, easeOutVal);                // Map it to the number of LED's you have.

  leds[lerpVal] = ColorFromPalette(palette, gHue + (easeInVal << 1), 40 + easeOutVal);
  fadeToBlackBy(leds, NUM_PIXELS, 16);                          // 8 bit, 1 = slow fade, 255 = fast fade
 
} // ease()
