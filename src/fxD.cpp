/**
 * Category D of light effects
 *
 */

#include "fxD.h"

//~ Global variables definition
uint8_t     fade = 8;                                        // How quickly does it fade? Lower = slower fade rate.
uint8_t     hue = 50;                                       // Starting hue.
uint8_t     incr = 1;                                        // Incremental value for rotating hues
uint8_t     saturation = 100;                                      // The saturation, where 255 = brilliant colours.
uint        hueDiff = 256;                                      // Range of random #'s to use for hue
uint8_t     dotBpm = 30;


void fxd_register() {
    static FxD1 fxD1;
    static FxD2 fxD2;
}

/**
 * Confetti
 * By: Mark Kriegsman
 * Modified By: Andrew Tuline
 * Date: July 2015
 *
 * Confetti flashes colours within a limited hue. It's been modified from Mark's original to support a few variables. It's a simple, but great looking routine.
 */
FxD1::FxD1() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxD1::setup() {
    FastLED.clear(true);
    FastLED.setBrightness(BRIGHTNESS);

    fade = 8;
    hue = 50;
    incr = 1;
    saturation = 100;
    brightness = 255;
    hueDiff = 256;
    speed = 5;
}

void FxD1::loop() {
    ChangeMe();                                                 // Check the demo loop for changes to the variables.

    EVERY_N_MILLISECONDS(speed) {                           // FastLED based non-blocking speed to update/display the sequence.
        confetti();
    }
    FastLED.show();
}

const char *FxD1::description() {
    return "FXD1: Confetti flashes colours within a limited hue. Simple, but great looking routine";
}

const char *FxD1::name() {
    return "FXD1";
}

void FxD1::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["brightness"] = brightness;
}


void FxD1::confetti() {                                             // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, NUM_PIXELS, fade);                    // Low values = slower fade.
  int pos = random16(NUM_PIXELS);                               // Pick an LED at random.
  leds[pos] += CHSV((hue + random16(hueDiff)) / 4 , saturation, brightness);  // I use 12 bits for hue so that the hue increment isn't too quick.
  hue = hue + incr;                                // It increments here.
} // confetti()


void FxD1::ChangeMe() {
    static uint8_t secSlot = 0;
    EVERY_N_SECONDS(5) {
        switch (secSlot) {
            case 0: incr=1; hue=192; saturation=255; fade=2; hueDiff=256; break;  // You can change values here, one at a time , or altogether.
            case 1: incr=2; hue=128; fade=8; hueDiff=64; break;
            case 2: incr=1; hue=random16(255); fade=1; hueDiff=16; break;
            default: break;
        }
        secSlot = inc(secSlot, 1, 4);
    }
}

/**
 * dots By: John Burroughs
 * Modified by: Andrew Tuline
 * Date: July, 2015
 *
 * Similar to dots by John Burroughs, but uses the FastLED beatsin8() function instead.
 */
FxD2::FxD2() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxD2::setup() {
    FastLED.clear(true);
    FastLED.setBrightness(BRIGHTNESS);
    dotBpm = 30;
    fade = 31;
    palette = paletteFactory.mainPalette();
}

void FxD2::loop() {
    EVERY_N_MILLISECONDS(100) {
        dot_beat();
        FastLED.show();
    }
}

const char *FxD2::description() {
    return "FXD2: dot beat";
}

const char *FxD2::name() {
    return "FXD2";
}

void FxD2::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["bpm"] = dotBpm;
    obj["fade"] = 255-fade;
}

void FxD2::dot_beat() {

  uint8_t inner = beatsin8(dotBpm, NUM_PIXELS/4, NUM_PIXELS/4*3);    // Move 1/4 to 3/4
  uint8_t outer = beatsin8(dotBpm, 0, NUM_PIXELS-1);               // Move entire length
  uint8_t middle = beatsin8(dotBpm, NUM_PIXELS/3, NUM_PIXELS/3*2);   // Move 1/3 to 2/3

  leds[middle] = ColorFromPalette(palette, 0);
  leds[inner] = ColorFromPalette(palette, 127);
  leds[outer] = ColorFromPalette(palette, 255);

  nscale8(leds, NUM_PIXELS, 255-fade);                             // Fade the entire array. Or for just a few LED's, use  nscale8(&leds[2], 5, fadeVal);

} // dot_beat()

