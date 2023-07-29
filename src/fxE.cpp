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
    static FxE3 fxE3;
    static FxE4 fxE4;
}

void fxe_setup() {
    FastLED.clear(true);
    FastLED.setBrightness(BRIGHTNESS);
    currentBlending = LINEARBLEND;
    palette = paletteFactory.secondaryPalette();
    targetPalette = paletteFactory.mainPalette();
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

    EVERY_N_SECONDS(10) {                                        // Change the target palette to a random one every 10 seconds.
        //static uint8_t baseC = random8();                         // You can use this as a baseline colour if you want similar hues in the next line.
        targetPalette = PaletteFactory::randomPalette();
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



void FxE1::ChangeMe() {
    static uint8_t secSlot = 0;
    EVERY_N_SECONDS(5) {
        switch (secSlot) {
            case 0: speed = 10; randhue = true; saturation=255; fade=8; twinkrate=150; break;  // You can change values here, one at a time , or altogether.
            case 1: speed = 100; randhue = false; hue=random8(); fade=2; twinkrate=20; break;
            default: break;
        }
        secSlot = inc(secSlot, 1, 3);
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
        targetPalette = PaletteFactory::randomPalette(0, millis());
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

void FxE3::setup() {
    fxe_setup();
}

void FxE3::loop() {
    int bpm = 60;
    int ms_per_beat = 60000/bpm;                                // 500ms per beat, where 60,000 = 60 seconds * 1000 ms
    int ms_per_led = 60000/bpm/NUM_PIXELS;

    int cur_led = ((millis() % ms_per_beat) / ms_per_led)%(NUM_PIXELS);     // Using millis to count up the strand, with %NUM_LEDS at the end as a safety factor.

    if (cur_led == 0)
        fill_solid(leds, NUM_PIXELS, CRGB::Black);
    else
        leds[cur_led] = ColorFromPalette(palette, 0, 255, currentBlending);    // I prefer to use palettes instead of CHSV or CRGB assignments.

    FastLED.show();
}

const char *FxE3::description() {
    return "FxE3: sawtooth";
}

const char *FxE3::name() {
    return "FxE3";
}

void FxE3::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

FxE3::FxE3() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxE4::setup() {
    fxe_setup();
    X = Xorig;
    Y = Yorig;
}

void FxE4::loop() {
    EVERY_N_SECONDS(5) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);  // Blend towards the target palette
    }

    EVERY_N_SECONDS(30) {
        targetPalette = PaletteFactory::randomPalette(random8());
    }

    EVERY_N_MILLISECONDS(50) {
        serendipitous();
        FastLED.show();
    }

}

const char *FxE4::description() {
    return "FxE4: serendipitous";
}

const char *FxE4::name() {
    return "FxE4";
}

void FxE4::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

FxE4::FxE4() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxE4::serendipitous() {
//  Xn = X-(Y/2); Yn = Y+(Xn/2);
//  Xn = X-Y/2;   Yn = Y+Xn/2;
//  Xn = X-(Y/2); Yn = Y+(X/2.1);
    Xn = X-(Y/3); Yn = Y+(X/1.5);
//  Xn = X-(2*Y); Yn = Y+(X/1.1);

    X = Xn;
    Y = Yn;

    index=(sin8(X)+cos8(Y))/2;                            // Guarantees maximum value of 255

    CRGB newcolor = ColorFromPalette(palette, index, 255, LINEARBLEND);

//  nblend(leds[X%NUM_LEDS-1], newcolor, 224);          // Try and smooth it out a bit. Higher # means less smoothing.
    nblend(leds[map(X,0,65535,0,NUM_PIXELS)], newcolor, 224); // Try and smooth it out a bit. Higher # means less smoothing.

    fadeToBlackBy(leds, NUM_PIXELS, 16);                    // 8 bit, 1 = slow, 255 = fast

}
