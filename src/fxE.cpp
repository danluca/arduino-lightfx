/**
 * Category E of light effects
 *
 */
#include "fxE.h"

//~ Global variables definition
int twinkrate = 100;                                     // The higher the value, the lower the number of twinkles.
bool randhue =   true;                                     // Do we want random colours all the time? 1 = yes.
TBlendType    currentBlending;

void fxe_register() {
    static FxE1 fxe1;
    static FxE2 fxE2;
}

void fxe_setup() {
    FastLED.clear(true);
    FastLED.setBrightness(BRIGHTNESS);
    currentBlending = LINEARBLEND;
    palette = paletteFactory.mainPalette();
    twinkrate = 100;
    speed =  10;
    fade =   8;
    hue =  50;
    saturation = 255;
    brightness = 255;
    randhue = true;
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
    fxe_setup();
}

void FxE1::loop() {
    ChangeMe();                                                 // Check the demo loop for changes to the variables.

    EVERY_N_MILLISECONDS(100) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);   // AWESOME palette blending capability.
    }

    EVERY_N_MILLISECONDS_I(fxe1Timer, speed) {                           // FastLED based non-blocking speed to update/display the sequence.
        twinkle();
        fxe1Timer.setPeriod(speed);
    }

    EVERY_N_SECONDS(5) {                                        // Change the target palette to a random one every 5 seconds.
        //static uint8_t baseC = random8();                         // You can use this as a baseline colour if you want similar hues in the next line.
        targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
    }

    FastLED.show();
}

const char *FxE1::description() {
    return "FXE1: twinkle";
}


void FxE1::twinkle() {

  if (random8() < twinkrate) leds[random16(NUM_PIXELS)] += ColorFromPalette(palette, (randhue ? random8() : hue), brightness, currentBlending);
  fadeToBlackBy(leds, NUM_PIXELS, fade);
  
} // twinkle()



void FxE1::ChangeMe() {                                             // A time (rather than loop) based demo sequencer. This gives us full control over the length of each sequence.

  uint8_t secondHand = (millis() / 1000) % 10;                // IMPORTANT!!! Change '15' to a different value to change duration of the loop.
  static uint8_t lastSecond = 99;                             // Static variable, means it's only defined once. This is our 'debounce' variable.
  if (lastSecond != secondHand) {                             // Debounce to make sure we're not repeating an assignment.
    lastSecond = secondHand;
    switch(secondHand) {
      case 0: speed = 10; randhue = true; saturation=255; fade=8; twinkrate=150; break;  // You can change values here, one at a time , or altogether.
      case 5: speed = 100; randhue = false; hue=random8(); fade=2; twinkrate=20; break;
      case 10: break;
    }
  }

}

const char *FxE1::name() {
    return "FXE1";
}

void FxE1::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["brightness"] = brightness;
}
// e01_ChangeMe()

//=====================================
FxE2::FxE2() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxE2::setup() {
    fxe_setup();
    targetPalette = paletteFactory.mainPalette();
    palette = paletteFactory.secondaryPalette();
}

void FxE2::loop() {
    beatwave();

    EVERY_N_MILLISECONDS(100) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);   // AWESOME palette blending capability.
    }

    EVERY_N_SECONDS(15) {                                        // Change the target palette to a random one every 15 seconds.
        targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
    }

    FastLED.show();
}

const char *FxE2::description() {
    return "FXE2: beatwave";
}


void FxE2::beatwave() {
  
  uint8_t wave1 = beatsin8(9, 0, 255);                        // That's the same as beatsin8(9);
  uint8_t wave2 = beatsin8(8, 0, 255);
  uint8_t wave3 = beatsin8(7, 0, 255);
  uint8_t wave4 = beatsin8(6, 0, 255);

  for (int i=0; i<NUM_PIXELS; i++) {
    leds[i] = ColorFromPalette(palette, i + wave1 + wave2 + wave3 + wave4, brightness, currentBlending);
  }
  
}

const char *FxE2::name() {
    return "FXE2";
}

void FxE2::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}
// beatwave()
