/**
 * Category C of light effects
 *
 */

#include "fxC.h"

void fxc_register() {
    static FxC1 fxC1;
    static FxC2 fxC2;
    static FxC3 fxC3;
}

void fxc_setup() {
    FastLED.clear(true);
    FastLED.setBrightness(BRIGHTNESS);
}

/**
 * aanimations
 * By: Can't recall where I found this. Maybe Stefan Petrick.
 * Modified by: Andrew Tuline
 *
 * Date: January, 2017
 * This sketch demonstrates how to blend between two animations running at the same time.
 */
FxC1::FxC1() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxC1::setup() {
    fxc_setup();
}

void FxC1::loop() {
    animationA();                                               // render the first animation into leds2
    animationB();                                               // render the second animation into leds3

    uint8_t ratio = beatsin8(2);                                // Alternate between 0 and 255 every minute

    for (int i = 0; i < NUM_PIXELS; i++) {                        // mix the 2 arrays together
        leds[i] = blend( leds2[i], leds3[i], ratio );
    }

    FastLED.show();
}

const char *FxC1::description() {
    return "FXC1: blend between two animations running at the same time - green / red moving bands in opposite directions";
}

void FxC1::animationA() {                                             // running red stripe.

  for (int i = 0; i < NUM_PIXELS; i++) {
    uint8_t red = (millis() / 10) + (i * 12);                    // speed, length
    if (red > 128) red = 0;
    leds2[i] = CRGB(red, 0, 0);
  }
} // animationA()



void FxC1::animationB() {                                               // running green stripe in opposite direction.
  for (int i = 0; i < NUM_PIXELS; i++) {
    uint8_t green = (millis() / 5) - (i * 12);                    // speed, length
    if (green > 128) green = 0;
    leds3[i] = CRGB(0, green, 0);
  }
}

const char *FxC1::name() {
    return "FXC1";
}

void FxC1::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}
// animationB()

//=====================================
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

FxC2::FxC2() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxC2::setup() {
    fxc_setup();
}

void FxC2::loop() {
    uint8_t blurAmount = dim8_raw( beatsin8(3,64, 192) );       // A sinewave at 3 Hz with values ranging from 64 to 192.
    blur1d( leds, NUM_PIXELS, blurAmount);                        // Apply some blurring to whatever's already on the strip, which will eventually go black.

    uint8_t  i = beatsin8(  9, 0, NUM_PIXELS);
    uint8_t  j = beatsin8( 7, 0, NUM_PIXELS);
    uint8_t  k = beatsin8(  5, 0, NUM_PIXELS);

    // The color of each point shifts over time, each at a different speed.
    uint16_t ms = millis();
    leds[(i+j)/2] = CHSV( ms / 29, 200, 255);
    leds[(j+k)/2] = CHSV( ms / 41, 200, 255);
    leds[(k+i)/2] = CHSV( ms / 73, 200, 255);
    leds[(k+i+j)/3] = CHSV( ms / 53, 200, 255);

    FastLED.show();
}

const char *FxC2::description() {
    return "FXC2: blur function. If you look carefully at the animation, sometimes there's a smooth gradient between each LED.";
}

const char *FxC2::name() {
    return "FXC2";
}

void FxC2::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

/**
 * inoise8_mover
 * By: Andrew Tuline
 * Date: February, 2017
 *
 * We've used sine waves and counting to move pixels around a strand. In this case, I'm using Perlin Noise to move a pixel up and down the strand.
 * The advantage here is that it provides random natural movement without requiring lots of fancy math by joe programmer.
 */
static int32_t dist;                                               // A moving location for our noise generator.
const uint32_t xscale = 8192;                                         // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
const uint32_t yscale = 7680;                                         // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
const uint8_t maxChanges = 24;                                      // Value for blending between palettes.

FxC3::FxC3() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxC3::setup() {
    fxc_setup();
    brightness = 128;
    palette = LavaColors_p;
    targetPalette = OceanColors_p;
    dist = random();
}

void FxC3::loop() {
    EVERY_N_MILLISECONDS(25) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);   // AWESOME palette blending capability.

        uint16_t locn = inoise16(xscale, dist+yscale) % 0xFFFF;           // Get a new pixel location from moving noise.
        uint16_t pixlen = map(locn,0,0xFFFF,0,NUM_PIXELS);                     // Map that to the length of the strand.
        leds[pixlen] = ColorFromPalette(palette, pixlen % 256, 255, LINEARBLEND);   // Use that value for both the location as well as the palette index colour for the pixel.

        dist += beatsin16(10,128,8192);                                                // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
        fadeToBlackBy(leds, NUM_PIXELS, 4);
    }

    EVERY_N_SECONDS(5) {                                        // Change the target palette to a random one every 5 seconds.
        targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
    }

    FastLED.show();
}

void FxC3::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["xscale"] = xscale;
    obj["yscale"] = yscale;
}

const char *FxC3::name() {
    return "FXC3";
}

const char *FxC3::description() {
    return "FXC3: using Perlin Noise to move a pixel up and down the strand for a random natural movement";
}
