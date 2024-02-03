//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
/**
 * Category E of light effects
 *
 */
#include "fxE.h"

using namespace FxE;
using namespace colTheme;

//~ Effect description strings stored in flash
const char fxe1Desc[] PROGMEM = "FXE1: twinkle";
const char fxe2Desc[] PROGMEM = "FXE2: beat wave";
const char fxe3Desc[] PROGMEM = "FxE3: sawtooth back/forth";
const char fxe4Desc[] PROGMEM = "FxE4: serendipitous";
const char fxe5Desc[] PROGMEM = "FxE5: three single color beat-waves";

void FxE::fxRegister() {
    static FxE1 fxe1;
    static FxE2 fxE2;
    static FxE3 fxE3;
    static FxE4 fxE4;
    static FxE5 fxE5;
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
FxE1::FxE1() : LedEffect(fxe1Desc) {}

void FxE1::setup() {
    LedEffect::setup();
    speed = 20;
    saturation = 255;
    brightness = 224;
}

void FxE1::run() {
    updateParams();                                                 // Check the demo loop for changes to the variables.

    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    EVERY_N_MILLISECONDS_I(fxe1Timer, speed) {                           // FastLED based non-blocking speed to update/display the sequence.
        twinkle();
        FastLED.show(stripBrightness);
        fxe1Timer.setPeriod(speed);
    }

    if (!paletteFactory.isHolidayLimitedHue()) {
        EVERY_N_SECONDS(10) {
            //static uint8_t baseC = random8();                         // You can use this as a baseline colour if you want similar hues in the next line.
            targetPalette = PaletteFactory::randomPalette();
        }
    }
}

void FxE1::twinkle() {

  if (random8() < twinkrate) leds[random16(NUM_PIXELS)] += ColorFromPalette(palette, (randhue ? random8() : hue), brightness, LINEARBLEND);
  fadeToBlackBy(leds, NUM_PIXELS, fade);
  
} // twinkle()

void FxE1::updateParams() {
    static uint8_t secSlot = 0;
    EVERY_N_SECONDS(5) {
        switch (secSlot) {
            case 0: speed = 25; randhue = true; saturation=255; fade=8; twinkrate=150; break;  // You can change values here, one at a time , or altogether.
            case 1: speed = 100; randhue = false; hue=random8(); fade=2; twinkrate=20; break;
            default: break;
        }
        secSlot = inc(secSlot, 1, 3);
    }
}

JsonObject & FxE1::describeConfig(JsonArray &json) const {
    JsonObject obj = LedEffect::describeConfig(json);
    obj["brightness"] = brightness;
    return obj;
}

bool FxE1::windDown() {
    return transEffect.offSpots();
}

uint8_t FxE1::selectionWeight() const {
    return 22;
}

// Fx E2
FxE2::FxE2() : LedEffect(fxe2Desc) {}

void FxE2::setup() {
    LedEffect::setup();
    targetPalette = paletteFactory.mainPalette();
    palette = paletteFactory.secondaryPalette();
    saturation = 255;
    brightness = 224;
}

void FxE2::run() {
    EVERY_N_MILLIS(100) {
        beatwave();
        FastLED.show(stripBrightness);
    }

    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    if (!paletteFactory.isHolidayLimitedHue()) {
        EVERY_N_SECONDS(15) {
            targetPalette = PaletteFactory::randomPalette(0, millis());
        }
    }
}

void FxE2::beatwave() {
    uint8_t wave1 = beatsin8(9, 0, 255);                        // That's the same as beatsin8(9);
    uint8_t wave2 = beatsin8(8, 0, 255);
    uint8_t wave3 = beatsin8(7, 0, 255);
    uint8_t wave4 = beatsin8(6, 0, 255);

    for (uint16_t i=0; i<tpl.size(); i++)
        tpl[i] = ColorFromPalette(palette, i + wave1 + wave2 + wave3 + wave4, brightness, LINEARBLEND);
    replicateSet(tpl, others);
}

void FxE2::windDownPrep() {
    transEffect.prepare(random8());
}

uint8_t FxE2::selectionWeight() const {
    return 17;
}

//Fx E3
FxE3::FxE3() : LedEffect(fxe3Desc), shdOverlay(frame(0, FRAME_SIZE-1)) {
}

void FxE3::setup() {
    LedEffect::setup();
    fade = dimmed;
    delta = 18;
    segStart = segEnd = 0;
    timerSlot = 0;
    cycles = 10;
    move = forward;
}

/**
 * This is overly complicated for the visual outcome. Either way, I got it to work as intended, but really
 * not worth it and too brittle.
 * Good thing that Nano RP2040 is powerful enough to make this fast.
 */
void FxE3::run() {
    EVERY_N_MILLISECONDS(60) {
        uint16_t maxIndex = tpl.size() - 1;
        if (timerSlot == 0) {
            switch (move) {
                case sasquatch:
                    //restore previous content on the sasquatch shadow position
                    if (segEnd < sasquatchSize-1) {
                        segStart = 0;
                        segEnd = curPos;
                    } else {
                        tpl[segStart] = shdOverlay[segStart];
                        segStart = curPos-sasquatchSize+1;
                        segEnd = curPos;
                        segEnd = capu(segEnd, maxIndex);
                    }
                    cycles = 1;
                    incr(curPos, 1, tpl.size()+sasquatchSize-1);
                    break;
                case forward:
                case backward:
                    uint16_t newPos = curPos + random8(1, 5);
                    newPos = capu(newPos, maxIndex);
                    if (move == forward) {
                        segStart = curPos;
                        segEnd = newPos;
                        cycles = random8(14, 30);
                        fade = dimmed;
                        delta = 18;
                    } else {
                        segStart = maxIndex - curPos;
                        segEnd = maxIndex - newPos;
                        cycles = 7;     //faster wiping clean
                        fade = dimmed*4;
                        delta = 40;
                    }
                    colorIndex = beatsin8(7);
                    curPos = newPos == maxIndex ? 0 : (newPos + 1);
                    break;
            }
        } else if (timerSlot == 25) {
            if (move == pauseF)
                tpl[segEnd] = shdOverlay[segEnd];
        }

        //fading in/out during light movement
        CRGBSet seg = tpl(segStart, segEnd);
        switch (move) {
            case forward: seg = ColorFromPalette(palette, colorIndex, fade, LINEARBLEND); break;
            case backward: seg.fadeToBlackBy(fade); break;
            case sasquatch: seg[seg.size()-1].fadeToBlackBy(252); break;
        }

        fade = capu(fade + delta, brightness);
        incr(timerSlot, 1, cycles);

        if ((timerSlot == 0) && (curPos == 0)) {
            //state machine of movement transitions
            switch (move) {
                case forward: move = sasquatch; segStart = segEnd = 0; shdOverlay = tpl; break;
                case sasquatch: move = pauseF; timerSlot = 1; cycles = 50; break;
                case pauseF: move = backward; shuffle(tpl); break;
                case backward: move = pauseB; timerSlot = 1; cycles = 30; break;
                case pauseB: move = forward; break;
            }
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

void FxE3::windDownPrep() {
    transEffect.prepare(random8());
}

uint8_t FxE3::selectionWeight() const {
    return 27;
}

//Fx E4
FxE4::FxE4() : LedEffect(fxe4Desc) {}

void FxE4::setup() {
    LedEffect::setup();
    X = Xorig;
    Y = Yorig;
}

void FxE4::run() {
    EVERY_N_SECONDS(2) {
        nblendPaletteTowardPalette(palette, targetPalette, maxChanges);
    }

    if (!paletteFactory.isHolidayLimitedHue()) {
        EVERY_N_SECONDS(30) {
            targetPalette = PaletteFactory::randomPalette(random8());
        }
    }

    EVERY_N_MILLISECONDS(50) {
        serendipitous();
        FastLED.show(stripBrightness);
    }
}

void FxE4::serendipitous() {
    //  Xn = X-(Y/2); Yn = Y+(Xn/2);
    //  Xn = X-Y/2;   Yn = Y+Xn/2;
    uint16_t Xn = X-(Y/2); uint16_t Yn = Y+(X/2.1); uint16_t Zn = X + Y*2.3;
    //    Xn = X-(Y/3); Yn = Y+(X/1.5);
    //  Xn = X-(2*Y); Yn = Y+(X/1.1);

    X = Xn;
    Y = Yn;

    index=(sin8(X)+cos8(Y))/2;
    CRGB newcolor = ColorFromPalette(palette, index, map(Zn, 0, 65535, dimmed*3, brightness), LINEARBLEND);

    nblend(tpl[map(X, 0, 65535, 0, tpl.size()-1)], newcolor, 224);    // Try and smooth it out a bit. Higher # means less smoothing.
    tpl.fadeToBlackBy(16);                    // 8 bit, 1 = slow, 255 = fast
    replicateSet(tpl, others);
}

void FxE4::windDownPrep() {
    transEffect.prepare(random8());
}

uint8_t FxE4::selectionWeight() const {
    return 36;
}

// FxE5
FxE5::FxE5() : LedEffect(fxe5Desc), wave2(frame(0, FRAME_SIZE-1)), wave3(frame(FRAME_SIZE, 2*FRAME_SIZE-1)) {
    clr1 = clr2 = clr3 = 0;
    pos2 = pos3 = 0;
}

void FxE5::setup() {
    LedEffect::setup();
    clr1 = random8();
    clr2 = clr1+64;
    clr3 = clr2+64;
}

void FxE5::run() {
    EVERY_N_MILLIS(30) {
        tpl.fadeToBlackBy(30);
        wave2.fadeToBlackBy(40);
        wave3.fadeToBlackBy(50);

        CRGB col1 = ColorFromPalette(palette, clr1, brightness, LINEARBLEND);
        CRGB col2 = ColorFromPalette(palette, clr2, brightness, LINEARBLEND);
        CRGB col3 = ColorFromPalette(palette, clr3, brightness, LINEARBLEND);
        uint16_t pos = beatsin16(5, 0, tpl.size()-1, 0, 0);
        tpl(curPos, pos) = col1;
        curPos = pos;
        pos = beatsin16(7, 0, tpl.size()-1, 0, 4096);
        wave2(pos2, pos) = col2;
        pos2 = pos;
        pos = beatsin16(11, 0, tpl.size()-1, 0, 8192);
        wave3(pos3, pos) = col3;
        pos3 = pos;

        blendScreen(wave2, wave3);
        tpl += wave2;
        replicateSet(tpl, others);

        FastLED.show(stripBrightness);
    }

    EVERY_N_SECONDS(27) {
        clr1 = clr2;
        clr2 = clr3;
        clr3 = random8();
    }

}

void FxE5::windDownPrep() {
    transEffect.prepare(random8());
}

uint8_t FxE5::selectionWeight() const {
    return 42;
}
