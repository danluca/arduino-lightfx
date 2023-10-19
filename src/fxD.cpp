/**
 * Category D of light effects
 *
 */

#include "fxD.h"

using namespace FxD;
using namespace colTheme;

//~ Effect description strings stored in flash
const char fxd1Desc[] PROGMEM = "FXD1: Confetti D";
const char fxd2Desc[] PROGMEM = "FXD2: dot beat";
const char fxd3Desc[] PROGMEM = "FxD3: plasma";
const char fxd4Desc[] PROGMEM = "FxD4: rainbow marching";
const char fxd5Desc[] PROGMEM = "FxD5: ripples";

void FxD::fxRegister() {
    static FxD1 fxD1;
    static FxD2 fxD2;
    static FxD3 fxD3;
    static FxD4 fxD4;
    static FxD5 fxD5;
}

/**
 * Confetti
 * By: Mark Kriegsman
 * Modified By: Andrew Tuline
 * Date: July 2015
 *
 * Confetti flashes colours within a limited hue. It's been modified from Mark's original to support a few variables. It's a simple, but great looking routine.
 */
FxD1::FxD1() = default;

void FxD1::setup() {
    resetGlobals();

    fade = 8;
    hue = 50;
    delta = 3;
    saturation = 224;
    brightness = 255;
    hueDiff = 512;
    speed = 30;
}

void FxD1::loop() {
    ChangeMe();                                                 // Check the demo loop for changes to the variables.

    EVERY_N_MILLISECONDS(speed) {                           // FastLED based non-blocking speed to update/display the sequence.
        confetti();
        FastLED.show(stripBrightness);
    }
}

const char *FxD1::description() const {
    return String(fxd1Desc).c_str();
}

inline const char *FxD1::name() const {
    return "FXD1";
}

JsonObject & FxD1::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["localBright"] = brightness;
    return obj;
}


void FxD1::confetti() {
    // random colored speckles that blink in and fade smoothly
    tpl.fadeToBlackBy(fade);                    // Low values = slower fade.
    uint16_t pos = random16(tpl.size());    // Pick an LED at random.
    //tpl[pos] += CHSV((hue + random16(hueDiff)) / 4 , saturation, localBright);  // I use 12 bits for hue so that the hue increment isn't too quick.
    tpl[pos] += ColorFromPalette(palette, hue, brightness, LINEARBLEND);
    hue = hue + delta;                                // It increments here.
    replicateSet(tpl, others);
}


void FxD1::ChangeMe() {
    static uint8_t secSlot = 0;
    EVERY_N_SECONDS(5) {
        switch (secSlot) {
            case 0: delta=1; hue=192; saturation=255; fade=2; hueDiff=256; break;  // You can change values here, one at a time , or altogether.
            case 1: delta=2; hue=128; fade=8; hueDiff=64; break;
            case 2: delta=3; hue=random16(255); fade=1; hueDiff=16; break;
            default: break;
        }
        secSlot = inc(secSlot, 1, 4);
    }
}

// Fx D2
/**
 * dots By: John Burroughs
 * Modified by: Andrew Tuline
 * Date: July, 2015
 *
 * Similar to dots by John Burroughs, but uses the FastLED beatsin8() function instead.
 */
FxD2::FxD2() = default;

void FxD2::setup() {
    resetGlobals();
    dotBpm = 21;
    fade = 31;
}

void FxD2::loop() {
    EVERY_N_MILLISECONDS(75) {
        dot_beat();
        FastLED.show(stripBrightness);
    }
}

const char *FxD2::description() const {
    return String(fxd2Desc).c_str();
}

inline const char *FxD2::name() const {
    return "FXD2";
}

JsonObject & FxD2::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["bpm"] = dotBpm;
    obj["fade"] = 255-fade;
    return obj;
}

void FxD2::dot_beat() {
    uint16_t inner = beatsin16(dotBpm, tpl.size()/4, tpl.size()/4*3);    // Move 1/4 to 3/4
    uint16_t outer = beatsin16(dotBpm, 0, tpl.size()-1);               // Move entire length
    uint16_t middle = beatsin16(dotBpm, tpl.size()/3, tpl.size()/3*2);   // Move 1/3 to 2/3

    tpl[middle] = ColorFromPalette(palette, beatsin8(10));
    tpl[inner] = ColorFromPalette(palette, beatsin8(11, 0, 255, 0, 127));
    tpl[outer] = ColorFromPalette(palette, beatsin8(12, 0, 255, 0, 255));

    tpl.nscale8(255-fade);                             // Fade the entire array. Or for just a few LED's, use  nscale8(&leds[2], 5, fadeVal);

    replicateSet(tpl, others);
}

// Fx D3
void FxD3::setup() {
    resetGlobals();
    targetPalette = paletteFactory.mainPalette();
    palette = paletteFactory.secondaryPalette();
}

void FxD3::loop() {
    EVERY_N_MILLISECONDS(50) {                                  // FastLED based non-blocking delay to update/display the sequence.
        plasma();
        FastLED.show(stripBrightness);
    }

    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    if (!paletteFactory.isHolidayLimitedHue()) {
        EVERY_N_SECONDS(20) {
            targetPalette = PaletteFactory::randomPalette(random8());
        }
    }

}

void FxD3::plasma() {
    uint8_t thisPhase = beatsin8(6,-64,64);                           // Setting phase change for a couple of waves.
    uint8_t thatPhase = beatsin8(7,-64,64);

    for (int k=0; k<NUM_PIXELS; k++) {                              // For each of the LED's in the strand, set a localBright based on a wave as follows:

        uint8_t colorIndex = cubicwave8((k*23)+thisPhase)/2 + cos8((k*15)+thatPhase)/2;           // Create a wave and add a phase change and add another wave with its own phase change.. Hey, you can even change the frequencies if you wish.
        uint8_t thisBright = qsuba(colorIndex, beatsin8(7,0,96));                                 // qsub gives it a bit of 'black' dead space by setting sets a minimum value. If colorIndex < current value of beatsin8(), then bright = 0. Otherwise, bright = colorIndex..

        leds[k] = ColorFromPalette(palette, colorIndex, thisBright, LINEARBLEND);  // Let's now add the foreground colour.
    }
}

const char *FxD3::description() const {
    return String(fxd3Desc).c_str();
}

inline const char *FxD3::name() const {
    return "FxD3";
}

FxD3::FxD3() = default;

// Fx D4
void FxD4::setup() {
    resetGlobals();
    hue = 0;
    hueDiff = 1;
}

void FxD4::loop() {
    static uint8_t secSlot = 0;

    EVERY_N_SECONDS(5) {
        update_params(secSlot);
        secSlot = inc(secSlot, 1, 15);
    }

    EVERY_N_MILLISECONDS(50) {
        rainbow_march();
        FastLED.show(stripBrightness);
    }
}

const char *FxD4::description() const {
    return String(fxd4Desc).c_str();
}

inline const char *FxD4::name() const {
    return "FxD4";
}

FxD4::FxD4() = default;

void FxD4::update_params(uint8_t slot) {
    switch (slot) {
        case 0: rot = 1; hueDiff = 5; break;
        case 2: dirFwd = true; hueDiff = 10; break;
        case 4: rot = 5; dirFwd=false; break;
        case 6: rot = 5; dirFwd = true;hueDiff = 20; break;
        case 9: hueDiff = 30; break;
        case 12: hueDiff = 2;rot = 5; break;
        default: break;
    }
}

void FxD4::rainbow_march() {
    if (dirFwd) hue += rot; else hue-= rot;                                       // I could use signed math, but 'dirFwd' works with other routines.
    if (paletteFactory.isHolidayLimitedHue())
        tpl.fill_gradient_RGB(ColorFromPalette(palette, hue, brightness),
          ColorFromPalette(palette, hue+128, brightness),
          ColorFromPalette(palette, 255-hue, brightness));
    else
        tpl.fill_rainbow(hue, hueDiff);           // I don't change hueDiff on the fly as it's too fast near the end of the strip.
    replicateSet(tpl, others);
}

// Fx D5
void FxD5::setup() {
    resetGlobals();
}

void FxD5::loop() {
    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }
    EVERY_N_MILLISECONDS(100) {
        ripples();
        FastLED.show(stripBrightness);
    }
}

const char *FxD5::description() const {
    return String(fxd5Desc).c_str();
}

inline const char *FxD5::name() const {
    return "FxD5";
}

void FxD5::ripples() {
    //fadeToBlackBy(leds, NUM_PIXELS, fade);                             // 8 bit, 1 = slow, 255 = fast
    for (auto & r : ripplesData) {
        if (random8() > 224 && !r.Alive()) {
            r.Init(&tpl);
        }
    }

    for (auto & r : ripplesData) {
        if (r.Alive()) {
            r.Fade();
            r.Move();
        }
    }
    replicateSet(tpl, others);
}

FxD5::FxD5() = default;

// ripple structure API
void ripple::Move() {
    if (step == 0) {
        (*pSeg)[center] = ColorFromPalette(palette, color, rpBright, LINEARBLEND);
    } else if (step < 12) {
        uint16_t x = (center + step) % pSeg->size();
        x = (center + step) >= pSeg->size() ? (pSeg->size() - x - 1) : x;        // we want the "wave" to bounce back from the end, rather than start from the other end
        (*pSeg)[x] += ColorFromPalette(palette, color + 16, rpBright*2/step, LINEARBLEND);       // Simple wrap from Marc Miller
        x = asub(center, step) % pSeg->size();
        (*pSeg)[x] += ColorFromPalette(palette, color + 16, rpBright*2/step, LINEARBLEND);
    }
    step++;  // Next step.
}

void ripple::Fade() const {
    uint16_t lowEndRipple = qsuba(center, step);
    uint16_t upEndRipple = capu(center + step, pSeg->size()-1);
    (*pSeg)(lowEndRipple, upEndRipple).fadeToBlackBy(rpFade);
}

bool ripple::Alive() const {
    return step < 42;
}

void ripple::Init(CRGBSet *set) {
    pSeg = set;
    center = random8(pSeg->size() / 8, pSeg->size() - pSeg->size() / 8);          // Avoid spawning too close to edge.
    rpBright = random8(192, 255);                                   // upper range of localBright
    color = random8();
    rpFade = random8(25, 80);
    step = 0;
}

