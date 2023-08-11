/**
 * Category A of light effects
 * 
 */
#include "fxA.h"

//~ Global variables definition for FxA
const CRGB BKG = CRGB::Black;
CRGBPalette16 palette;
uint8_t brightness = 224;
uint8_t colorIndex = 0;
uint8_t lastColorIndex = 0;

volatile uint16_t speed = 100;
volatile uint16_t curPos = 0;
OpMode mode = Chase;
uint16_t szStack = 0;
uint16_t stripShuffleIndex[NUM_PIXELS];
CRGB frame[NUM_PIXELS];

void fxa_register() {
    static FxA1 fxA1;
    static FxA2 fxA2;
    static FxA3 fxA3;
    static FxA4 fxA4;
    static FxA5 fxA5;
}

void fxa_setup() {
    FastLED.clear(true);
    FastLED.setBrightness(BRIGHTNESS);
    fill_solid(frame, NUM_PIXELS, BKG);

    palette = paletteFactory.mainPalette();
    //shuffle led indexes
    shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
    reset();
    colorIndex = lastColorIndex = 0;
    curPos = 0;
    speed = 100;
}

///////////////////////////////////////
// Support Utilities
///////////////////////////////////////

void stack(CRGB color, CRGB dest[], uint16_t stackStart, uint16_t szStackSeg) {
    for (int x = 0; x < szStackSeg; x++) {
        dest[stackStart + x] = color;
    }
}

uint16_t fxa_incStackSize(int16_t delta, uint16_t max) {
    szStack = capr(szStack + delta, 0, max);
    return szStack;
}

uint16_t fxa_stackAdjust(CRGB array[], uint16_t szArray, uint16_t szStackSeg) {
    uint16_t minStack = szStackSeg << 1;
    if (szStack < minStack) {
        fxa_incStackSize(szStackSeg, szArray);
        return szStack;
    }
    if (colorIndex < 16) {
        shiftRight(array, szArray, 1);
        fxa_incStackSize(-1, szArray);
    } else if (inr(colorIndex, 16, 32) || colorIndex == lastColorIndex) {
        fxa_incStackSize((int16_t)-szStackSeg, szArray);
    } else
        fxa_incStackSize(szStackSeg, szArray);
    return szStack;
}


void reset() {
    fill_solid(frame, FRAME_SIZE, BKG);
    szStack = 0;
    mode = Chase;
}

///////////////////////////////////////
// Effect Definitions - setup and loop
///////////////////////////////////////
//FX A1
FxA1::FxA1() {
    registryIndex = fxRegistry.registerEffect(this);
    fill_solid(dot, MAX_DOT_SIZE, BKG);
}

void FxA1::setup() {
    fxa_setup();
    makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed + 50, brightness), LINEARBLEND), szSegment);
}

void FxA1::makeDot(CRGB color, uint16_t szDot) {
    for (uint16_t x = 0; x < szDot; x++) {
        dot[x] = color;
        dot[x] %= brightness;
    }
    dot[szDot-1] = color;
    dot[szDot-1] %= dimmed;
}

void FxA1::loop() {
    if (mode == TurnOff) {
        if (turnOff())
            reset();
        return;
    }
    EVERY_N_MILLISECONDS_I(a1Timer, speed) {
        uint16_t upLimit = FRAME_SIZE - szStack + szStackSeg;
        shiftRight(frame, FRAME_SIZE, upLimit, 1, curPos < szSegment ? dot[curPos] : BKG);
        if (curPos >= (upLimit-szStackSeg))
            stack(dot[0], frame, upLimit, szStackSeg);
        showFill(frame, FRAME_SIZE);

        curPos = inc(curPos, 1, upLimit);
        //update speed if in next segment of 16 leds
        if (!bConstSpeed && (curPos % 16 == 15) && random8(0, 2)) {
            speed = random16(30, 211);
        }
        a1Timer.setPeriod(speed);
        if (curPos == 0) {
            fxa_stackAdjust(frame, FRAME_SIZE, szStackSeg);
            mode = szStack == FRAME_SIZE ? TurnOff : Chase;
            //save the color
            lastColorIndex = colorIndex;
            colorIndex = random8();
            makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed + 60, brightness), LINEARBLEND), szSegment);
        }
    }
}


const char *FxA1::description() {
    return "FXA1: Multiple Tetris segments";
}

void FxA1::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["segmentSize"] = szSegment;
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());;
}

const char *FxA1::name() {
    return "FXA1";
}


// FX A2
FxA2::FxA2() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxA2::setup() {
    fxa_setup();
    makeDot();
}

void FxA2::makeDot() {
    dot[szSegment-1] = ColorFromPalette(palette, 255, dimmed, LINEARBLEND);
    for (uint16_t x = 1; x < (szSegment-1); x++) {
        dot[x] = ColorFromPalette(palette, colorIndex + random8(32), brightness, LINEARBLEND);
    }
    dot[0] = ColorFromPalette(palette, 255, 255, LINEARBLEND);
}

void FxA2::loop() {
    if (mode == TurnOff) {
        if (turnOff()) reset();
        return;
    }
    EVERY_N_MILLISECONDS_I(a2Timer, speed) {
        static CRGB feed = BKG;
        switch (movement) {
            case forward:
                shiftRight(leds, NUM_PIXELS, NUM_PIXELS, 1, feed);
                speed = beatsin8(1, 50, 200);
                break;
            case backward:
                shiftLeft(leds, NUM_PIXELS, NUM_PIXELS, 1, feed);
                speed = beatsin8(258, 40, 200);
                break;
            case pause: speed = 5000; movement = backward; break;
        }
        FastLED.show();

        curPos = inc(curPos, 1, NUM_PIXELS);
        uint8_t ss = curPos % (szSegment+spacing);
        if (ss == 0) {
            //change segment, space sizes
            szSegment = beatsin8(3, 1, 7);
            spacing = beatsin8(8, 1, 11);
            lastColorIndex = colorIndex;
            colorIndex = beatsin8(10);
        } else if (ss < szSegment)
            feed = ColorFromPalette(palette, colorIndex, beatsin8(6, dimmed, brightness, curPos), LINEARBLEND);
        else
            feed = BKG;

        if (curPos == 0)
            movement = forward;
        else if (curPos == (NUM_PIXELS - NUM_PIXELS/4))
            movement = pause;
        a2Timer.setPeriod(speed);
    }

    EVERY_N_SECONDS(61) {
        mode = TurnOff;
    }
}

const char *FxA2::description() {
    return "FXA2: Tetris, fixed size dot randomly spaced";
}

void FxA2::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["segmentSize"] = szSegment;
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());;
}

const char *FxA2::name() {
    return "FXA2";
}

//=====================================
//FX A3
FxA3::FxA3() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxA3::setup() {
    fxa_setup();
    makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed + 50, brightness), LINEARBLEND), szSegment);
    bFwd = true;
}

void FxA3::makeDot(CRGB color, uint16_t szDot) {
    for (uint16_t x = 1; x < szDot; x++) {
        dot[x] = color;
        dot[x] %= brightness;
    }
    dot[szDot-1] = color;
    dot[szDot-1] %= dimmed;
}

void FxA3::loop() {
    EVERY_N_MILLISECONDS_I(a3Timer, speed) {
        uint16_t szViewport = FRAME_SIZE;
        if (bFwd)
            shiftRight(frame, FRAME_SIZE, szViewport, 1, curPos < szSegment ? dot[curPos] : BKG);
        else
            shiftLeft(frame, FRAME_SIZE, szViewport, 1, (szViewport - curPos) < szSegment ? dot[szViewport-curPos] : BKG);
        if (bFwd && (curPos >= (szViewport - szStackSeg)))
            frame[szViewport - 1] = dot[szSegment - 1];
        showFill(frame, FRAME_SIZE);
        if (bFwd) curPos++; else curPos--;
        if (curPos == 0) {
            bFwd = !bFwd;
            lastColorIndex = colorIndex;
            colorIndex = random8();
            speed = random8(30, 211);
            szSegment = random8(2, MAX_DOT_SIZE);
            makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed + 50, brightness), LINEARBLEND), szSegment);
        }
        if (curPos == szViewport)
            bFwd= !bFwd;

        a3Timer.setPeriod(speed);
    }
}

const char *FxA3::description() {
    return "FXA3: Moving variable dot size back and forth on a frame, repeat across whole strip";
}

void FxA3::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());;
}

const char *FxA3::name() {
    return "FXA3";
}

// FX A4
void FxA4::setup() {
    szSegment = 2;
    fxa_setup();
    palette = paletteFactory.mainPalette();
    szStackSeg = 1;
    mode = Chase;
}

void FxA4::loop() {
    if (mode == TurnOff) {
        if (turnOff())
            reset();
        return;
    }

//    EVERY_N_MILLIS(speed << 2) {
//        FastLED.setBrightness(random8(40, 225));
//    }

    EVERY_N_MILLISECONDS_I(a4Timer, speed) {
        uint upLimit = FRAME_SIZE - szStack;
        setTrailColor(frame, curPos,
                      ColorFromPalette(palette, colorIndex, random8(dimmed + 50, brightness), LINEARBLEND),
                      brightness, dimmed);
        if (curPos == (upLimit - 1))
            frame[curPos - 1] = frame[curPos];
        pushFrame(frame, FRAME_SIZE, 0, true);
        fadeLightBy(leds, NUM_PIXELS, random8(0, 133)); //add a flicker
        FastLED.show();

        // Turn our current led back to black for the next loop around
        if (curPos < (upLimit - 1)) {
            offTrailColor(frame, curPos);
        }
        curPos = inc(curPos, 1, upLimit);
        if (curPos == 0) {
            fxa_stackAdjust(frame, FRAME_SIZE, szStackSeg);
            mode = szStack == FRAME_SIZE ? TurnOff : Chase;
            lastColorIndex = colorIndex;
            colorIndex = random8();
            speed = random16(30, 211);
        }
        a4Timer.setPeriod(speed);
    }
}

const char *FxA4::description() {
    return "FXA4: 36 pixel segments, moving 1 pixel dot flickering";
}

FxA4::FxA4() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxA4::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());;
}

const char *FxA4::name() {
    return "FXA4";
}

// FX A5
void FxA5::setup() {
    szSegment = 1;
    fxa_setup();
    palette = paletteFactory.mainPalette();
    szStackSeg = 1;
    mode = Chase;
}

void FxA5::loop() {
    if (mode == TurnOff) {
        if (turnOff()) {
            fill_solid(leds, NUM_PIXELS, BKG);
            lastColorIndex = colorIndex = szStack = 0;
            mode = Chase;
        }
        return;
    }

    EVERY_N_MILLISECONDS_I(a5Timer, speed) {
        uint upLimit = NUM_PIXELS - szStack;
        setTrailColor(leds, curPos,
                      ColorFromPalette(palette, colorIndex, random8(dimmed + 50, brightness), LINEARBLEND),
                      brightness, dimmed);
        if (curPos == (upLimit - 1))
            leds[curPos - 1] = leds[curPos];
        FastLED.show();

        if (curPos < (upLimit - 1)) {
            offTrailColor(leds, curPos);
        }
        curPos = inc(curPos, 1, upLimit);
        if (curPos == 0) {
            fxa_stackAdjust(leds, NUM_PIXELS, szStackSeg);
            mode = szStack == NUM_PIXELS ? TurnOff : Chase;
            lastColorIndex = colorIndex;
            colorIndex = random8();
            speed = random16(25, 221);
            a5Timer.setPeriod(speed);
        }
    }
}

const char *FxA5::description() {
    return "FXA5: Moving single pixel dot with stacking";
}

FxA5::FxA5() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxA5::describeConfig(JsonArray &json) {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());;
}

const char *FxA5::name() {
    return "FXA5";
}

