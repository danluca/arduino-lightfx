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
CRGBArray<NUM_PIXELS> frame;

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
    brightness = 224;
}

///////////////////////////////////////
// Support Utilities
///////////////////////////////////////

void stack(CRGB color, CRGBSet& dest, uint16_t stackStart, uint16_t szStackSeg) {
    dest(stackStart, stackStart+szStackSeg) = color;
}

uint16_t fxa_incStackSize(int16_t delta, uint16_t max) {
    szStack = capr(szStack + delta, 0, max);
    return szStack;
}

uint16_t fxa_stackAdjust(CRGBSet& set, uint16_t szStackSeg) {
    uint16_t minStack = szStackSeg << 1;
    if (szStack < minStack) {
        fxa_incStackSize(szStackSeg, set.size());
        return szStack;
    }
    if (colorIndex < 16) {
        shiftRight(set, BKG);
        fxa_incStackSize(-1, set.size());
    } else if (inr(colorIndex, 16, 32) || colorIndex == lastColorIndex) {
        fxa_incStackSize((int16_t)-szStackSeg, set.size());
    } else
        fxa_incStackSize(szStackSeg, set.size());
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
FxA1::FxA1() : dot(frame(0, FRAME_SIZE)) {
    registryIndex = fxRegistry.registerEffect(this);
    dot.fill_solid(BKG);
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
        uint16_t upLimit = FRAME_SIZE - szStack;
        CRGBSet tpl(leds, FRAME_SIZE);
        CRGBSet others(leds, FRAME_SIZE, NUM_PIXELS);
        shiftRight(tpl, curPos < szSegment ? dot[curPos] : BKG, (Viewport) upLimit);
        if (curPos >= (upLimit-szStackSeg))
            stack(dot[0], tpl, upLimit, szStackSeg);
        replicateSet(tpl, others);

        if (paletteFactory.currentHoliday() == Halloween)
            fadeLightBy(leds, NUM_PIXELS, random8(0, 133)); //add a flicker

        FastLED.show();

        curPos = inc(curPos, 1, upLimit);
        if (curPos == 0) {
            fxa_stackAdjust(tpl, szStackSeg);
            mode = szStack == FRAME_SIZE ? TurnOff : Chase;
            //save the color
            lastColorIndex = colorIndex;
            colorIndex = random8();
            makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed + 60, brightness), LINEARBLEND), szSegment);
            speed = random16(40, 81);
            a1Timer.setPeriod(speed);
        }
    }
}


const char *FxA1::description() const {
    return "FXA1: Multiple Tetris segments";
}

void FxA1::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["segmentSize"] = szSegment;
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());;
}

const char *FxA1::name() const {
    return "FXA1";
}


// FX A2
FxA2::FxA2() : dot(frame(0, FRAME_SIZE)) {
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
        if (turnOffJuggle()) reset();
        return;
    }
    EVERY_N_MILLISECONDS_I(a2Timer, speed) {
        static CRGB feed = BKG;
        CRGBSet tpl(leds, FRAME_SIZE);
        CRGBSet others(leds, FRAME_SIZE, NUM_PIXELS);
        switch (movement) {
            case forward:
                shiftRight(tpl, feed);
                speed = beatsin8(1, 60, 150);
                break;
            case backward:
                shiftLeft(tpl,feed);
                speed = beatsin8(258, 50, 150);
                break;
            case pause: speed = 3000; movement = backward; break;
        }
        replicateSet(tpl, others);
        if (paletteFactory.currentHoliday() == Halloween)
            fadeLightBy(leds, NUM_PIXELS, random8(0, 133)); //add a flicker
        FastLED.show();

        incr(curPos, 1, NUM_PIXELS);
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

const char *FxA2::description() const {
    return "FXA2: Randomly sized and spaced segments moving on entire strip";
}

void FxA2::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["segmentSize"] = szSegment;
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());;
}

const char *FxA2::name() const {
    return "FXA2";
}

//=====================================
FxA3::FxA3() : dot(frame(0, FRAME_SIZE)) {
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
        CRGBSet tpl(leds, FRAME_SIZE);
        CRGBSet others(leds, FRAME_SIZE, NUM_PIXELS);
        if (bFwd)
            shiftRight(tpl, curPos < szSegment ? dot[curPos] : BKG);
        else
            shiftLeft(tpl, (FRAME_SIZE - curPos) < szSegment ? dot[FRAME_SIZE-curPos] : BKG);
        replicateSet(tpl, others);
        if (paletteFactory.currentHoliday() == Halloween)
            fadeLightBy(leds, NUM_PIXELS, random8(0, 133)); //add a flicker
        FastLED.show();

        if (bFwd) curPos++; else curPos--;
        if (curPos == 0) {
            bFwd = !bFwd;
            lastColorIndex = colorIndex;
            colorIndex = random8();
            speed = random8(40, 171);
            a3Timer.setPeriod(speed);
            szSegment = random8(2, 16);
            makeDot(ColorFromPalette(palette, colorIndex, random8(dimmed + 50, brightness), LINEARBLEND), szSegment);
        }
        if (curPos == (FRAME_SIZE+szSegment-szStackSeg))
            bFwd= !bFwd;

    }
}

const char *FxA3::description() const {
    return "FXA3: Moving variable dot size back and forth on a frame, repeat across whole strip";
}

void FxA3::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());;
}

const char *FxA3::name() const {
    return "FXA3";
}

// FX A4
void FxA4::setup() {
    szSegment = 3;
    fxa_setup();
    palette = paletteFactory.mainPalette();
    szStackSeg = 2;
    brightness = 192;
    mode = Chase;
    curBkg = BKG;
    makeDot(ColorFromPalette(palette, colorIndex, brightness, LINEARBLEND), szSegment);
}

void FxA4::makeDot(CRGB color, uint16_t szDot) {
    CRGB c1 = color;
    CRGB c2 = ColorFromPalette(palette, colorIndex<<2, brightness, LINEARBLEND);
    dot(0, szDot).fill_gradient_RGB(c1, c2);
}

void FxA4::loop() {
    if (mode == TurnOff) {
        if (turnOff())
            reset();
        return;
    }

    EVERY_N_MILLISECONDS_I(a4Timer, speed) {
        CRGBSet tplR(leds, FRAME_SIZE);
        CRGBSet tplL = frame(0, FRAME_SIZE);
        CRGBSet others(leds, FRAME_SIZE, NUM_PIXELS);
        curBkg = ColorFromPalette(palette, beatsin8(11), 4, LINEARBLEND);
        uint8_t ss = curPos % (szSegment+spacing);
        shiftRight(tplR, ss < szSegment ? dot[ss] : curBkg);
        shiftLeft(tplL, curPos < szStackSeg ? ColorFromPalette(palette, -colorIndex, brightness, LINEARBLEND) : BKG);
        replicateSet(tplR, others);
        for (uint16_t x = 0; x < NUM_PIXELS; x++) {
            uint8_t xf = x%FRAME_SIZE;
            leds[x] += (leds[x] > curBkg) && tplL[xf] ? CRGB::White : tplL[xf];
            if (xf > (FRAME_SIZE-curPos-szStackSeg))
                leds[x].fadeToBlackBy(48);
        }

        if (paletteFactory.currentHoliday() == Halloween)
            fadeLightBy(leds, NUM_PIXELS, random8(0, 133)); //add a flicker
        FastLED.show();


        incr(curPos, 1, FRAME_SIZE);
        if (curPos == 0) {
            lastColorIndex = colorIndex;
            colorIndex = random8();
            szSegment = beatsin8(21, 3, 7);
            spacing = beatsin8(15, szStackSeg+4, 22);
            speed = random16(40, 171);
            a4Timer.setPeriod(speed);
            makeDot(ColorFromPalette(palette, colorIndex, brightness, LINEARBLEND), szSegment);
        }
    }

    EVERY_N_SECONDS(61) {
        mode = TurnOff;
    }
}

const char *FxA4::description() const {
    return "FXA4: pixel segments moving opposite directions";
}

FxA4::FxA4() : dot(frame(0, FRAME_SIZE)) {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxA4::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());;
}

const char *FxA4::name() const {
    return "FXA4";
}

void FxA5::setup() {
    fxa_setup();
    palette = paletteFactory.mainPalette();
    targetPalette = paletteFactory.secondaryPalette();
    makeFrame();
    mode = Chase;
}

void FxA5::makeFrame() {
    ovr.fill_solid(ColorFromPalette(targetPalette, colorIndex, brightness-64, LINEARBLEND));
    CRGB newClr = ColorFromPalette(palette, colorIndex, brightness, LINEARBLEND);
    for (CRGB& pix : ovr) {
        nblend(pix,newClr, (&pix-ovr.leds)<<4);
    }
}

void FxA5::loop() {
    if (mode == TurnOff) {
        if (turnOff()) {
            lastColorIndex = colorIndex = szStack = 0;
            mode = Chase;
        }
        return;
    }

    EVERY_N_MILLISECONDS_I(a5Timer, speed) {
        CRGBSet tpl(leds, FRAME_SIZE);
        CRGBSet others(leds, FRAME_SIZE, NUM_PIXELS);
        shiftRight(tpl, ovr[curPos]);
        replicateSet(tpl, others);
        FastLED.show();

        curPos = inc(curPos, 1, FRAME_SIZE);
        if (curPos == 0) {
            lastColorIndex = colorIndex;
            colorIndex = random8();
            makeFrame();
            speed = 2000;
            a5Timer.setPeriod(speed);
        } else if (curPos == 1) {
            speed = random16(40, 131);
            a5Timer.setPeriod(speed);
        }
    }

    EVERY_N_SECONDS(61) {
        mode = TurnOff;
    }
}

const char *FxA5::description() const {
    return "FXA5: Moving a color swath on top of another";
}

FxA5::FxA5() : ovr(frame(0, FRAME_SIZE)) {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxA5::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    obj["palette"] = holidayToString(paletteFactory.currentHoliday());;
}

const char *FxA5::name() const {
    return "FXA5";
}

