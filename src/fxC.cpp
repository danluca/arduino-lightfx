/**
 * Category C of light effects
 *
 */

#include "fxC.h"

void fxc_register() {
    static FxC1 fxC1;
    static FxC2 fxC2;
    static FxC3 fxC3;
    static FxC4 fxC4;
    static FxC5 fxC5;
    static FxC6 fxC6;
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
    palette = paletteFactory.mainPalette();
    brightness = 176;
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

const char *FxC1::description() const {
    return "FXC1: blend between two animations running at the same time";
}

void FxC1::animationA() {                                             // running red stripe.

  for (int i = 0; i < NUM_PIXELS; i++) {
    uint8_t red = (millis() / 10) + (i * 12);                    // speed, length
    if (red > 128) red = 0;
    //leds2[i] = CRGB(red, 0, 0);
    leds2[i] = ColorFromPalette(palette, red, dim8_raw(red << 1), LINEARBLEND);
  }
} // animationA()



void FxC1::animationB() {                                               // running green stripe in opposite direction.
  for (int i = 0; i < NUM_PIXELS; i++) {
    uint8_t green = (millis() / 5) - (i * 12);                    // speed, length
    if (green > 128) green = 0;
//    leds3[i] = CRGB(0, green, 0);
    leds3[i] = ColorFromPalette(palette, 255-green, dim8_raw(green << 1), LINEARBLEND);
  }
}  // animationB()

const char *FxC1::name() const {
    return "FXC1";
}

void FxC1::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

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

    uint16_t  i = beatsin16(  9, 0, NUM_PIXELS);
    uint16_t  j = beatsin16( 7, 0, NUM_PIXELS);
    uint16_t  k = beatsin16(  5, 0, NUM_PIXELS);

    // The color of each point shifts over time, each at a different speed.
    uint16_t ms = millis();
    leds[(i+j)/2] = CHSV( ms / 29, 200, 255);
    leds[(j+k)/2] = CHSV( ms / 41, 200, 255);
    leds[(k+i)/2] = CHSV( ms / 73, 200, 255);
    leds[(k+i+j)/3] = CHSV( ms / 53, 200, 255);

    FastLED.show();
}

const char *FxC2::description() const {
    return "FXC2: blur function";
}

const char *FxC2::name() const {
    return "FXC2";
}

void FxC2::describeConfig(JsonArray &json) const {
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
    targetPalette = paletteFactory.mainPalette();
    palette = paletteFactory.secondaryPalette();
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
        FastLED.show();
    }

    EVERY_N_SECONDS(5) {                                        // Change the target palette to a random one every 5 seconds.
        targetPalette = PaletteFactory::randomPalette();
    }

}

void FxC3::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["xscale"] = xscale;
    obj["yscale"] = yscale;
}

const char *FxC3::name() const {
    return "FXC3";
}

const char *FxC3::description() const {
    return "FXC3: using Perlin Noise to move a pixel up and down the strand for a random natural movement";
}

FxC4::FxC4() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxC4::setup() {
    fxc_setup();
    palette = paletteFactory.mainPalette();
    brightness = BRIGHTNESS;
}

void FxC4::loop() {
    static uint8_t frequency = 50;                                       // controls the interval between strikes
    static uint8_t flashes = 12;                                          //the upper limit of flashes per strike
    static uint dimmer = 1;
    static uint8_t ledstart;                                             // Starting location of a flash
    static uint8_t ledlen;                                               // Length of a flash

    ledstart = random8(NUM_PIXELS);                               // Determine starting location of flash
    ledlen = random8(NUM_PIXELS-ledstart);                        // Determine length of flash (not to go beyond NUM_LEDS-1)

    for (int flashCounter = 0; flashCounter < random8(5,flashes); flashCounter++) {
        if(flashCounter == 0) dimmer = 5;                         // the brightness of the leader is scaled down by a factor of 5
        else dimmer = random8(1,3);                               // return strokes are brighter than the leader

        CRGB color = ColorFromPalette(palette, random8(), brightness/dimmer, LINEARBLEND);
        fill_solid(leds+ledstart,ledlen,color);
        FastLED.show();                       // Show a section of LED's
        delay(random8(4,10));                                     // each flash only lasts 4-10 milliseconds
        fill_solid(leds+ledstart,ledlen,BKG);           // Clear the section of LED's
        FastLED.show();

        if (flashCounter == 0) delay (250);                       // longer speed until next flash after the leader

        delay(50+random8(100));                                   // shorter speed between strokes
    } // for()

    delay(random8(frequency)*100);                              // speed between strikes
}

const char *FxC4::description() const {
    return "FxC4: lightnings";
}

const char *FxC4::name() const {
    return "FxC4";
}

void FxC4::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

void FxC5::setup() {
    fxc_setup();
    palette = paletteFactory.secondaryPalette();
    targetPalette = paletteFactory.mainPalette();
    saturation = 255;
    speed = 50;
    brightness = BRIGHTNESS;
}

void FxC5::loop() {
    changeParams();

    EVERY_N_SECONDS(1) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    EVERY_N_MILLISECONDS_I(c5Timer, speed) {
        matrix();
        c5Timer.setPeriod(speed);
        FastLED.show();
    }

}

const char *FxC5::description() const {
    return "FxC5: matrix";
}

const char *FxC5::name() const {
    return "FxC5";
}

void FxC5::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

FxC5::FxC5() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxC5::changeParams() {
    static uint8_t secSlot = 0;
    EVERY_N_SECONDS(5) {
        switch(secSlot) {
            case 0: speed=50; palIndex=95; bgClr=140; bgBri=4; hueRot=true; break;
            case 1: targetPalette = paletteFactory.mainPalette(); fwd=false; bgBri=0; hueRot=true; break;
            case 2: targetPalette = paletteFactory.secondaryPalette(); speed=30; palIndex=0; bgClr=50; bgBri=8; hueRot=false; fwd=true; break;
            case 3: targetPalette = PaletteFactory::randomPalette(0, millis()); speed=80; bgBri = 16; bgClr=96; palIndex=random8(); break;
            case 4: palIndex=random8(); hueRot=true; break;
            default: break;
        }

        secSlot = inc(secSlot, 1, 5);   // Change '5' upper bound to a different value to change length of the loop.
    }
}

void FxC5::matrix() {
    if (hueRot) palIndex++;

    if (random8(90) > 80)
        leds[fwd?0:(NUM_PIXELS-1)] = ColorFromPalette(palette, palIndex, brightness, LINEARBLEND);
    else
        leds[fwd?0:(NUM_PIXELS-1)] = CHSV(bgClr, saturation, bgBri);

    if (fwd)
        for (int i = NUM_PIXELS-1; i >0 ; i-- ) leds[i] = leds[i-1];
    else
        for (int i = 0; i < NUM_PIXELS-1 ; i++ ) leds[i] = leds[i+1];
}

void FxC6::setup() {
    fxc_setup();
    speed = 8;
    fwd = true;
    palette = paletteFactory.secondaryPalette();
    targetPalette = paletteFactory.mainPalette();
}

void FxC6::loop() {
    static uint8_t secSlot = 0;

    EVERY_N_MILLISECONDS_I(c6Timer, delay) {
        one_sine_pal(millis()>>4);
        FastLED.show();
        c6Timer.setPeriod(delay);
    }

    EVERY_N_SECONDS(1) {
        fwd = secSlot == 6;
        if (fwd)
            nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
        secSlot = inc(secSlot, 1, 7);
    }


//    EVERY_N_SECONDS(5) {                                          // Change the target palette to a random one every 5 seconds.
//        static uint8_t baseC = random8();                         // You can use this as a baseline colour if you want similar hues in the next line.
//        targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
//    }
}

const char *FxC6::description() const {
    return "FxC6: one sine";
}

const char *FxC6::name() const {
    return "FxC6";
}

void FxC6::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

FxC6::FxC6() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxC6::one_sine_pal(uint8_t colorIndex) {
    // This is the heart of this program. Sure is short.
    phase = fwd ? phase - speed : phase + speed;

    for (int k=0; k<NUM_PIXELS; k++) {                                          // For each of the LED's in the strand, set a brightness based on a wave as follows:
        int thisBright = qsubd(cubicwave8((k * allfreq) + phase), cutoff);         // qsub sets a minimum value called thiscutoff. If < thiscutoff, then bright = 0. Otherwise, bright = 128 (as defined in qsub)..
        leds[k] = CHSV(bgclr, 255, bgbright);                                     // First set a background colour, but fully saturated.
        leds[k] += ColorFromPalette(palette, colorIndex, thisBright, LINEARBLEND);    // Let's now add the foreground colour.
        colorIndex +=3;
    }
    bgclr++;
}
