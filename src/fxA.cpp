/**
 * Category A of light effects
 * 
 */
#include "fxA.h"

//~ Global variables definition for FxA
CRGBPalette16 palette;
uint8_t brightness = 175;
uint8_t colorIndex = 0;
uint8_t lastColorIndex = 0;
volatile uint speed = 100;
uint8_t szSegment = 3;
uint8_t szStackSeg = szSegment >> 1;
uint szStack = 0;
bool bConstSpeed = true;
OpMode mode = Chase;
uint stripShuffleIndex[NUM_PIXELS];
CRGB dot[MAX_DOT_SIZE];
CRGB frame[NUM_PIXELS];
volatile uint curPos = 0;
const CRGB BKG = CRGB::Black;

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
    fill_solid(dot, MAX_DOT_SIZE, BKG);
    fill_solid(frame, NUM_PIXELS, BKG);

    palette = paletteFactory.mainPalette();
    //shuffle led indexes
    shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
    reset();
    colorIndex = lastColorIndex = 0;
    curPos = 0;
    speed = 100;
    szStack = 0;
    brightness = 175;
    szSegment = validateSegmentSize(szSegment);
    szStackSeg = szSegment >> 1;
    mode = Chase;
}

///////////////////////////////////////
// Support Utilities
///////////////////////////////////////
CRGB *makeDot(CRGB color, uint szDot) {
    dot[0] = color;
    dot[0] %= dimmed;
    for (uint x = 1; x < szDot; x++) {
        dot[x] = color;
        dot[x] %= brightness;
    }
    return dot;
}

bool isInViewport(int ledIndex, int viewportSize) {
    int viewportLow = 0;
    int viewportHi = viewportSize;
    return (ledIndex >= viewportLow) && (ledIndex < viewportHi);
}

uint validateSegmentSize(uint segSize) {
    return max(min(segSize, MAX_DOT_SIZE), 2u);
}

void moveSeg(const CRGB dotSeg[], uint szDot, CRGB dest[], uint lastPos, uint newPos, uint viewport) {
    bool rightDir = newPos >= lastPos;
    uint bkgSeg = min(szDot, abs(newPos - lastPos));
    for (uint x = 0; x < bkgSeg; x++) {
        uint lx = rightDir ? lastPos + x : newPos + szDot + x;
        if (isInViewport(lx, viewport))
            dest[lx] = BKG;
    }
    for (uint x = 0; x < szDot; x++) {
        uint lx = newPos + x;
        if (isInViewport(lx, viewport + szStackSeg))
            dest[lx] = dotSeg[rightDir ? x : szDot - 1 - x];
    }
}

void stack(CRGB color, CRGB dest[], uint stackStart) {
    for (int x = 0; x < szStackSeg; x++) {
        dest[stackStart + x] = color;
    }
}

uint fxa_incStackSize(int delta, uint max) {
    szStack = capr(szStack + delta, 0, max);
    return szStack;
}

uint fxa_stackAdjust(CRGB array[], uint szArray) {
    uint minStack = szSegment << 1;
    if (szStack < minStack) {
        fxa_incStackSize(szStackSeg, szArray);
        return szStack;
    }
    if (colorIndex < 16) {
        shiftRight(array, szArray, 1);
        fxa_incStackSize(-1, szArray);
    } else if (inr(colorIndex, 16, 32) || colorIndex == lastColorIndex) {
        fxa_incStackSize(-szStackSeg, szArray);
    } else
        fxa_incStackSize(szStackSeg, szArray);
    return szStack;
}


void moldWindow() {
    CRGB top[FRAME_SIZE];
    CRGB right[FRAME_SIZE];
    cloneArray(frame, top, FRAME_SIZE);
    fill_solid(right, FRAME_SIZE, BKG);
    copyArray(frame, 5, right, 5, 14);
    reverseArray(right, FRAME_SIZE);
    pushFrame(frame, 17);
    pushFrame(top, 19, 17);
    pushFrame(right, 14, 36);
}

void reset() {
    fill_solid(frame, FRAME_SIZE, BKG);
    szStack = 0;
    mode = Chase;
}

bool turnOff() {
    static uint led = 0;
    static uint xOffNow = 0;
    static uint szOffNow = turnOffSeq[xOffNow];
    static bool setOff = false;
    static bool allOff = false;

    EVERY_N_MILLISECONDS(25) {
        int totalLum = 0;
        for (uint x = 0; x < szOffNow; x++) {
            uint xled = stripShuffleIndex[(led + x) % FastLED.size()];
            FastLED.leds()[xled].fadeToBlackBy(12);
            totalLum += FastLED.leds()[xled].getLuma();
        }
        FastLED.show();
        setOff = totalLum < 4;
    }

    EVERY_N_MILLISECONDS(1200) {
        if (setOff) {
            led = (led + szOffNow) % FastLED.size();
            xOffNow = capu(xOffNow + 1, sizeof(turnOffSeq) / sizeof(int) - 1);
            szOffNow = turnOffSeq[xOffNow];
            setOff = false;
        }
        allOff = !isAnyLedOn(FastLED.leds(), FastLED.size(), CRGB::Black);
    }
    //if we're turned off all LEDs, reset the static variables for next time
    if (allOff) {
        led = 0;
        xOffNow = 0;
        szOffNow = turnOffSeq[xOffNow];
        setOff = false;
        allOff = false;
        return true;
    }

    return false;
}

///////////////////////////////////////
// Effect Definitions - setup and loop
///////////////////////////////////////
//FX A1
FxA1::FxA1() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxA1::setup() {
    fxa_setup();
}

void FxA1::loop() {
    if (mode == TurnOff) {
//        if (turnOff()) {
        if (turnOffJuggle()) {
            reset();
        }
        return;
    }
    colorIndex = random8();
    speed = random16(25, 201);
    int szViewport = FRAME_SIZE - szStack;
    int startIndex = 0 - szSegment;

    // Make a dot with current color
    // makeDot(palette[colorIndex], szSegment);
    makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed + 24, brightness), LINEARBLEND), szSegment);
    for (int led = startIndex; led < szViewport; led++) {
        //move the segment, build up stack
        moveSeg(dot, szSegment, frame, led, led + 1, szViewport);
        if (led == (szViewport - 1))
            stack(dot[szSegment - 1], frame, szViewport);

        // Show the updated leds
        //FastLED.show();
        //showFill(frame, FRAME_SIZE);
        moldWindow();
        FastLED.show();

        // Wait a little bit
        delay(speed);

        //update speed if in next segment of 10 leds
        if (!bConstSpeed && (led % 10 == 9) && random16(0, 2)) {
            speed = random16(25, 201);
        }
    }

    fxa_stackAdjust(frame, FRAME_SIZE);
    mode = szStack == FRAME_SIZE ? TurnOff : Chase;

    //save the color
    lastColorIndex = colorIndex;
}

const char *FxA1::description() {
    return "FXA1: Multiple Tetris segments, molded for office window";
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
void FxA2::setup() {
    szSegment = 5;
    fxa_setup();
    //szStackSeg = szSegment >> 1;
}

void FxA2::loop() {
    if (mode == TurnOff) {
        if (turnOff()) {
            fill_solid(frame, NUM_PIXELS, BKG);
            reset();
        }
        return;
    }
    colorIndex = random8();
    speed = random16(25, 201);
    int szViewport = NUM_PIXELS - szStack;
    int startIndex = 0 - szSegment;

    // Make a dot with current color
    // makeDot(palette[colorIndex], szSegment);
    makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed + 24, brightness), LINEARBLEND), szSegment);
    for (int led = startIndex; led < szViewport; led++) {
        //move the segment, build up stack
        moveSeg(dot, szSegment, frame, led, led + 1, szViewport);
        if (led == (szViewport - 1))
            stack(dot[szSegment - 1], frame, szViewport);

        // Show the updated leds
        //FastLED.show();
        showFill(frame, NUM_PIXELS);

        // Wait a little bit
        delay(speed);

        //update speed if in next segment of 10 leds
        if (!bConstSpeed && (led % 10 == 9) && random16(0, 2)) {
            speed = random16(25, 201);
        }
    }

    fxa_stackAdjust(frame, NUM_PIXELS);
    mode = szStack == NUM_PIXELS ? TurnOff : Chase;

    //save the color
    lastColorIndex = colorIndex;
}

const char *FxA2::description() {
    return "FXA2: Tetris, one big segment, fixed dot size - 5";
}

FxA2::FxA2() {
    registryIndex = fxRegistry.registerEffect(this);
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
void FxA3::setup() {
    fxa_setup();
    palette = paletteFactory.mainPalette();
}

void FxA3::loop() {
    if (mode == TurnOff) {
        if (turnOff()) {
            reset();
        }
        return;
    }

    colorIndex = random8();
    speed = random16(25, 201);
    szSegment = random8(2, MAX_DOT_SIZE);
    int szViewport = FRAME_SIZE;
    int startIndex = 0 - szSegment;

    // Make a dot with current color
    makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed + 24, brightness), LINEARBLEND), szSegment);
    //move dot to right
    for (int led = startIndex; led < szViewport; led++) {
        //move the segment, build up stack
        moveSeg(dot, szSegment, frame, led, led + 1, szViewport);
        //retain the last pixel for turning back
        if (led >= (szViewport - 2))
            frame[szViewport - 1] = dot[szSegment - 1];
        // Show the updated leds
        showFill(frame, FRAME_SIZE);
        // Wait a little bit
        delay(speed);
        //update speed if in next segment of 10 leds
        if (!bConstSpeed && (led % 10 == 9) && random16(0, 2)) {
            speed = random16(25, 201);
        }
    }
    //move dot back to left
    for (int led = szViewport; led > startIndex; led--) {
        //move the segment, build up stack
        moveSeg(dot, szSegment, frame, led, led - 1, szViewport);
        // Show the updated leds
        showFill(frame, FRAME_SIZE);
        // Wait a little bit
        delay(speed);
        //update speed if in next segment of 10 leds
        if (!bConstSpeed && (led % 10 == 9) && random16(0, 2)) {
            speed = random16(25, 201);
        }
    }

    //save the color
    lastColorIndex = colorIndex;
}

const char *FxA3::description() {
    return "FXA3: Moving variable dot size back and forth on the whole strip";
}

FxA3::FxA3() {
    registryIndex = fxRegistry.registerEffect(this);
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
        if (turnOff()) {
            fill_solid(frame, FRAME_SIZE, BKG);
            colorIndex = 0;
            lastColorIndex = 0;
            szStack = 0;
            mode = Chase;
        }
        return;
    }

//    EVERY_N_MILLIS(speed << 2) {
//        FastLED.setBrightness(random8(40, 225));
//    }

    EVERY_N_MILLISECONDS_I(a4Timer, speed) {
        uint upLimit = FRAME_SIZE - szStack;
        setTrailColor(frame, curPos,
                      ColorFromPalette(palette, colorIndex, random8(dimmed + 24, brightness), LINEARBLEND),
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
            fxa_stackAdjust(frame, FRAME_SIZE);
            lastColorIndex = colorIndex;
            colorIndex = random8();
            speed = random16(25, 201);
            a4Timer.setPeriod(speed);
            mode = szStack == FRAME_SIZE ? TurnOff : Chase;
        }
    }
}

const char *FxA4::description() {
    return "FXA4: 3 Segments, Tetris moving single pixel dot";
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
                      ColorFromPalette(palette, colorIndex, random8(dimmed + 24, brightness), LINEARBLEND),
                      brightness, dimmed);
        if (curPos == (upLimit - 1))
            leds[curPos - 1] = leds[curPos];
        FastLED.show();

        if (curPos < (upLimit - 1)) {
            offTrailColor(leds, curPos);
        }
        curPos = inc(curPos, 1, upLimit);
        if (curPos == 0) {
            fxa_stackAdjust(leds, NUM_PIXELS);
            lastColorIndex = colorIndex;
            colorIndex = random8();
            speed = random16(25, 221);
            a5Timer.setPeriod(speed);
            mode = szStack == NUM_PIXELS ? TurnOff : Chase;
        }
    }
}

const char *FxA5::description() {
    return "FXA5: Similar with FXA4, moving single pixel dot with stacking; implementation with timers";
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

