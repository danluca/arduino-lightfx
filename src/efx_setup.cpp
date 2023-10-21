#include "efx_setup.h"
#include "log.h"

//~ Global variables definition
#define JSON_DOC_SIZE   512
const uint16_t turnOffSeq[] PROGMEM = {1, 1, 2, 2, 2, 3, 3, 3, 5, 5, 5, 7, 7, 7, 7, 10};
const uint8_t dimmed = 20;
const char csAutoFxRoll[] = "autoFxRoll";
const char csStripBrightness[] = "stripBrightness";
const char csAudioThreshold[] = "audioThreshold";
const char csColorTheme[] = "colorTheme";
const char csAutoColorAdjust[] = "autoColorAdjust";
const char csRandomSeed[] = "randomSeed";
const char csCurFx[] = "curFx";

//const uint16_t FRAME_SIZE = 68;     //NOTE: frame size must be at least 3 times less than NUM_PIXELS. The frame CRGBSet must fit at least 3 frames
const CRGB BKG = CRGB::Black;
const uint8_t maxChanges = 24;
volatile bool fxBump = false;
volatile uint16_t speed = 100;
volatile uint16_t curPos = 0;

EffectRegistry fxRegistry;
CRGB leds[NUM_PIXELS];
CRGBArray<NUM_PIXELS> frame;
CRGBSet tpl(leds, FRAME_SIZE);                        //array length, indexes go from 0 to length-1
CRGBSet others(leds, tpl.size(), NUM_PIXELS-1); //start and end indexes are inclusive
CRGBPalette16 palette;
CRGBPalette16 targetPalette;
OpMode mode = Chase;
uint8_t brightness = 224;
uint8_t stripBrightness = brightness;
bool stripBrightnessLocked = false;
uint8_t colorIndex = 0;
uint8_t lastColorIndex = 0;
uint8_t fade = 8;
uint8_t hue = 50;
uint8_t delta = 1;
uint8_t saturation = 100;
uint8_t dotBpm = 30;
uint8_t twinkrate = 100;
uint16_t szStack = 0;
uint16_t stripShuffleIndex[NUM_PIXELS];
uint16_t hueDiff = 256;
uint16_t totalAudioBumps = 0;
int8_t rot = 1;
int32_t dist = 1;
bool dirFwd = true;
bool randhue = true;
float minVcc = 12.0f;
float maxVcc = 0.0f;
float minTemp = 100.0f;
float maxTemp = 0.0f;

//~ Support functions -----------------
/**
 * Setup the on-board status LED
 */
void setupStateLED() {
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);
    stateLED(CRGB::Black);
}
/**
 * Controls the on-board status LED
 * @param color
 */
void stateLED(CRGB color) {
    analogWrite(LEDR, 255 - color.r);
    analogWrite(LEDG, 255 - color.g);
    analogWrite(LEDB, 255 - color.b);
}
/**
 * Setup the strip LED lights to be controlled by FastLED library
 */
void ledStripInit() {
    FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_PIXELS).setCorrection(TypicalSMD5050).setTemperature(Tungsten100W);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear(true);
}

void readState() {
    String json;
    size_t stateSize = readTextFile(stateFileName, &json);
    if (stateSize > 0) {
        StaticJsonDocument<JSON_DOC_SIZE> doc; //this takes memory from the thread stack, ensure fx thread's memory size is adjusted if this value is
        deserializeJson(doc, json);

        bool autoAdvance = doc[csAutoFxRoll].as<bool>();
        fxRegistry.autoRoll(autoAdvance);

        uint16_t seed = doc[csRandomSeed].as<uint16_t>();
        random16_set_seed(seed);

        uint16_t fx = doc[csCurFx].as<uint16_t>();
        fxRegistry.nextEffectPos(fx);

        stripBrightness = doc[csStripBrightness].as<uint8_t>();

        audioBumpThreshold = doc[csAudioThreshold].as<uint16_t>();
        String savedHoliday = doc[csColorTheme].as<String>();
        paletteFactory.forceHoliday(colTheme::parseHoliday(&savedHoliday));
        bool autoColAdj = doc[csAutoColorAdjust].as<bool>();
        paletteFactory.setAuto(autoColAdj);

        Log.infoln(F("System state restored from %s [%d bytes]: autoFx=%s, randomSeed=%d, nextEffect=%d, brightness=%d (auto adjust), audioBumpThreshold=%d, holiday=%s"),
                   stateFileName, stateSize, autoAdvance ? "true" : "false", seed, fx, stripBrightness, audioBumpThreshold, savedHoliday.c_str());
    }
}

void saveState() {
    StaticJsonDocument<JSON_DOC_SIZE> doc;    //this takes memory from the thread stack, ensure fx thread's memory size is adjusted if this value is
    doc[csRandomSeed] = random16_get_seed();
    doc[csAutoFxRoll] = fxRegistry.isAutoRoll();
    doc[csCurFx] = fxRegistry.curEffectPos();
    doc[csStripBrightness] = stripBrightness;
    doc[csAudioThreshold] = audioBumpThreshold;
    doc[csColorTheme] = colTheme::holidayToString(paletteFactory.currentHoliday());
    doc[csAutoColorAdjust] = paletteFactory.isAuto();
    String str;
    serializeJson(doc, str);
    if (!writeTextFile(stateFileName, &str))
        Log.errorln(F("Failed to create/write the status file %s"), stateFileName);
}

//~ General Utilities ---------------------------------------------------------
/**
 * Resets the state of all global variables, including the state of the LED strip. Suitable for a clean slate
 * start of a new Fx
 * <p>This needs to account for ALL global variables</p>
 */
void resetGlobals() {
    //turn off the LEDs on the strip and the frame buffer
    FastLED.clear(true);
    FastLED.setBrightness(BRIGHTNESS);
    frame.fill_solid(BKG);

    palette = paletteFactory.mainPalette();
    targetPalette = paletteFactory.secondaryPalette();
    mode = Chase;
    brightness = 224;
    colorIndex = lastColorIndex = 0;
    curPos = 0;
    speed = 100;
    fade = 8;
    hue = 50;
    delta = 1;
    saturation = 100;
    dotBpm = 30;
    twinkrate = 100;
    szStack = 0;
    hueDiff = 256;
    rot = 1;
    dist = 1;
    dirFwd = true;
    randhue = true;
    fxBump = false;

    //shuffle led indexes
    shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
}

/**
 * Shifts the content of an array to the right by the number of positions specified
 * First item of the array (arr[0]) is used as seed to fill the new elements entering left
 * @param arr array
 * @param szArr size of the array
 * @param vwp limits of the shifting area
 * @param pos how many positions to shift right
 * @param feedLeft the color to introduce from the left as we shift the array
 */
void shiftRight(CRGBSet &set, CRGB feedLeft, Viewport vwp, uint16_t pos) {
    if ((pos == 0) || (set.size() == 0) || (vwp.low >= set.size()))
        return;
    if (vwp.size() == 0)
        vwp = (Viewport)set.size();
    uint16_t hiMark = capu(vwp.high, set.size());
    if (pos >= vwp.size()) {
        set(vwp.low, hiMark).fill_solid(feedLeft);
        return;
    }
    for (uint16_t x = hiMark; x > vwp.low; x--) {
        uint16_t y = x - 1;
        set[y] = y < pos ? feedLeft : set[y-pos];
    }
}

/**
 * Shifts the contents of an CRGBSet to the right in a circular buffer manner - the elements falling off to the right
 * are entering through the left
 * @param set the set
 * @param vwp limits of the shifting area
 * @param pos how many positions to shift right
 */
void loopRight(CRGBSet &set, Viewport vwp, uint16_t pos) {
    if ((pos == 0) || (set.size() == 0) || (vwp.low >= set.size()))
        return;
    if (vwp.size() == 0)
        vwp = (Viewport)set.size();
    uint16_t hiMark = capu(vwp.high, set.size());
    pos = pos % vwp.size();
    CRGB buf[pos];
    for (uint16_t x = hiMark; x > vwp.low; x--) {
        uint16_t y = x - 1;
        if (hiMark-x < pos)
            buf[hiMark-x] = set[y];
        set[y] = y < pos ? buf[pos-y-1] : set[y-pos];
    }
}

/**
 * Shifts the content of an array to the left by the number of positions specified
 * The elements entering right are filled with current's array last value (arr[szArr-1])
 * @param arr array
 * @param szArr size of the array
 * @param vwp limits of the shifting area
 * @param pos how many positions to shift left
 * @param feedRight the color to introduce from the right as we shift the array
 */
void shiftLeft(CRGBSet &set, CRGB feedRight, Viewport vwp, uint16_t pos) {
    if ((pos == 0) || (set.size() == 0) || (vwp.low >= set.size()))
        return;
    if (vwp.size() == 0)
        vwp = (Viewport)set.size();
    uint16_t hiMark = capu(vwp.high, set.size());
    if (pos >= vwp.size()) {
        set(vwp.low, hiMark).fill_solid(feedRight);
        return;
    }
    for (uint16_t x = vwp.low; x < hiMark; x++) {
        uint16_t y = x + pos;
        set[x] = y < set.size() ? set[y] : feedRight;
    }
}

/**
 * Replicate the source set into destination, repeating it as necessary to fill the entire destination
 * <p>Any overlaps between source and destination are skipped from replication - source set backing array is guaranteed unchanged</p>
 * @param src source set
 * @param dest destination set
 */
void replicateSet(const CRGBSet& src, CRGBSet& dest) {
    uint16_t srcSize = abs(src.len);    //src.size() would be more appropriate, but function is not marked const
    CRGB* normSrcStart = src.len < 0 ? src.end_pos : src.leds;     //src.reversed() would have been consistent, but function is not marked const
    CRGB* normSrcEnd = src.len < 0 ? src.leds : src.end_pos;
    CRGB* normDestStart = dest.reversed() ? dest.end_pos : dest.leds;
    CRGB* normDestEnd = dest.reversed() ? dest.leds : dest.end_pos;
    uint16_t x = 0;
    if (max(normSrcStart, normDestStart) < min(normSrcEnd, normDestEnd)) {
        //we have overlap - account for it
        for (uint16_t y = 0; y < dest.size(); y++) {
            CRGB* yPtr = &dest[y];
            if ((yPtr < normSrcStart) || (yPtr >= normSrcEnd)) {
                (*yPtr) = src[x];
                incr(x, 1, srcSize);
            }
        }
    } else {
        //no overlap - simpler assignment code
        for (uint16_t y = 0; y < dest.size(); y++) {
            dest[y] = src[x];
            incr(x, 1, srcSize);
        }
    }
}

/**
 * Populates and shuffles randomly an array of numbers in the range of 0-szArray (array indexes)
 * @param array array of indexes
 * @param szArray size of the array
 */
void shuffleIndexes(uint16_t array[], uint16_t szArray) {
    //populates the indexes in ascending order
    for (uint16_t x = 0; x < szArray; x++) {
        array[x] = x;
    }
    //shuffle indexes
    uint16_t swIter = (szArray >> 1) + (szArray >> 3);
    for (uint16_t x = 0; x < swIter; x++) {
        uint16_t r = random16(0, szArray);
        uint16_t tmp = array[x];
        array[x] = array[r];
        array[r] = tmp;
    }
}

//copy arrays using memcpy (arguably the fastest way) - no checks are made on the length copied vs actual length of both arrays
void copyArray(const CRGB *src, CRGB *dest, uint16_t length) {
    memcpy(dest, src, sizeof(src[0]) * length);
}

// copy arrays using pointer loops - one of the faster ways. No checks are made on the validity of offsets, length for both arrays
void copyArray(const CRGB *src, uint16_t srcOfs, CRGB *dest, uint16_t destOfs, uint16_t length) {
    const CRGB *srSt = src + srcOfs;
    CRGB *dsSt = &dest[destOfs];
    for (uint x = 0; x < length; x++) {
        *dsSt++ = *srSt++;
    }
}

uint16_t countLedsOn(CRGBSet *set, CRGB backg) {
    uint8_t bkgLuma = backg.getLuma();
    uint16_t ledsOn = 0;
    for (auto p: *set)
        if (p.getLuma() > bkgLuma)
            ledsOn++;

    return ledsOn;
}

bool isAnyLedOn(CRGB *arr, uint16_t szArray, CRGB backg) {
    uint8_t bkgLuma = backg.getLuma();
    for (uint x = 0; x < szArray; x++) {
        if (arr[x].getLuma() > bkgLuma)
            return true;
    }
    return false;
}

bool isAnyLedOn(CRGBSet *set, CRGB backg) {
    if (backg == CRGB::Black)
        return (*set);
    return isAnyLedOn(set->leds, set->size(), backg);
}

void fillArray(const CRGB *src, uint16_t srcLength, CRGB *array, uint16_t arrLength, uint16_t arrOfs) {
    size_t curFrameIndex = arrOfs;
    while (curFrameIndex < arrLength) {
        size_t len = capu(curFrameIndex + srcLength, arrLength) - curFrameIndex;
        copyArray(src, 0, array, curFrameIndex, len);
        curFrameIndex += srcLength;
    }
}

void mirrorLow(CRGBSet &set) {
    int swaps = set.size()/2;
    for (int x = 0; x < swaps; x++) {
        set[set.size() - x - 1] = set[x];
    }
}

void mirrorHigh(CRGBSet &set) {
    int swaps = set.size()/2;
    for (int x = 0; x < swaps; x++) {
        set[x] = set[set.size() - x - 1];
    }
}

/**
 * Adjust the brightness of the given color
 * <p>Code borrowed from <code>ColorFromPalette</code> function</p>
 * @param color color to change
 * @param bright brightness to apply
 * @return new color instance with desired brightness
 */
CRGB adjustBrightness(CRGB color, uint8_t bright) {
    //inspired from ColorFromPalette implementation of applying brightness factor
    uint8_t r = color.r, g = color.g, b = color.b;
    if (bright != 255) {
        if (bright) {
            ++bright; // adjust for rounding
            // Now, since localBright is nonzero, we don't need the full scale8_video logic;
            // we can just to scale8 and then add one (unless scale8 fixed) to all nonzero inputs.
            if (r)
                r = scale8_LEAVING_R1_DIRTY(r, bright);
            if (g)
                g = scale8_LEAVING_R1_DIRTY( g, bright);
            if (b)
                b = scale8_LEAVING_R1_DIRTY( b, bright);
            cleanup_R1();
        } else
            r = g = b = 0;
    }
    return {r, g, b};
}

/**
 * Blend multiply 2 colors
 * @param blendLayer base color, which is also the target (the one receiving the result)
 * @param topLayer color to multiply with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void blendMultiply(CRGB &blendRGB, const CRGB &topRGB) {
    blendRGB.r = bmul8(blendRGB.r, topRGB.r);
    blendRGB.g = bmul8(blendRGB.g, topRGB.g);
    blendRGB.b = bmul8(blendRGB.b, topRGB.b);
}

/**
 * Blend multiply 2 color sets
 * <p>Wherever either layer was brighter than black, the composite is darker; since each value is less than 1,
 * their product will be less than each initial value that was greater than zero</p>
 * @param blendLayer base color set, which is also the target set (the one receiving the result)
 * @param topLayer color set to multiply with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void blendMultiply(CRGBSet &blendLayer, const CRGBSet &topLayer) {
    for (CRGBSet::iterator bt = blendLayer.begin(), tp=topLayer.begin(), btEnd = blendLayer.end(), tpEnd = topLayer.end(); bt != btEnd && tp != tpEnd; ++bt, ++tp)
        blendMultiply(*bt, *tp);
}

/**
 * Blend screen 2 colors
 * @param blendLayer base color, which is also the target (the one receiving the result)
 * @param topLayer color to screen with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void blendScreen(CRGB &blendRGB, const CRGB &topRGB) {
    blendRGB.r = bscr8(blendRGB.r, topRGB.r);
    blendRGB.g = bscr8(blendRGB.g, topRGB.g);
    blendRGB.b = bscr8(blendRGB.b, topRGB.b);
}

/**
 * Blend screen 2 color sets
 * <p>The result is the opposite of Multiply: wherever either layer was darker than white, the composite is brighter. </p>
 * @param blendLayer base color set, which is also the target set (the one receiving the result)
 * @param topLayer color set to screen with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void blendScreen(CRGBSet &blendLayer, const CRGBSet &topLayer) {
    for (CRGBSet::iterator bt = blendLayer.begin(), tp=topLayer.begin(), btEnd = blendLayer.end(), tpEnd = topLayer.end(); bt != btEnd && tp != tpEnd; ++bt, ++tp)
        blendScreen(*bt, *tp);
}

/**
 * Blend overlay 2 colors
 * @param blendLayer base color, which is also the target (the one receiving the result)
 * @param topLayer color to overlay with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void blendOverlay(CRGB &blendRGB, const CRGB &topRGB) {
    blendRGB.r = bovl8(blendRGB.r, topRGB.r);
    blendRGB.g = bovl8(blendRGB.g, topRGB.g);
    blendRGB.b = bovl8(blendRGB.b, topRGB.b);
}

/**
 * Blend overlay 2 color sets
 * <p>Where the base layer is light, the top layer becomes lighter; where the base layer is dark, the top becomes darker; where the base layer is mid grey,
 * the top is unaffected. An overlay with the same picture looks like an S-curve. </p>
 * @param blendLayer base color set, which is also the target set (the one receiving the result)
 * @param topLayer color set to overlay with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void blendOverlay(CRGBSet &blendLayer, const CRGBSet &topLayer) {
    for (CRGBSet::iterator bt = blendLayer.begin(), tp=topLayer.begin(), btEnd = blendLayer.end(), tpEnd = topLayer.end(); bt != btEnd && tp != tpEnd; ++bt, ++tp)
        blendOverlay(*bt, *tp);
}

/**
 * Turns off entire strip by random spots, in increasing size until all leds are off
 * <p>Implemented with timers, no delay - that is why this function needs called repeatedly until it returns true</p>
 * @return true if all leds are off, false otherwise
 */
bool turnOffSpots() {
    static uint16_t led = 0;
    static uint16_t xOffNow = 0;
    static uint16_t szOffNow = turnOffSeq[xOffNow];
    static bool setOff = false;
    bool allOff = false;

    EVERY_N_MILLISECONDS(25) {
        uint8_t ledsOn = 0;
        for (uint16_t x = 0; x < szOffNow; x++) {
            uint16_t xled = stripShuffleIndex[(led + x) % NUM_PIXELS];
            FastLED.leds()[xled].fadeToBlackBy(36);
            if (FastLED.leds()[xled].getLuma() < 4)
                FastLED.leds()[xled] = BKG;
            else
                ledsOn++;
        }
        FastLED.show(stripBrightness);
        setOff = ledsOn == 0;
    }

    EVERY_N_MILLISECONDS(500) {
        if (setOff) {
            led = inc(led, szOffNow, NUM_PIXELS);
            xOffNow = capu(xOffNow + 1, arrSize(turnOffSeq) - 1);
            szOffNow = turnOffSeq[xOffNow];
            setOff = false;
        }
        allOff = !isAnyLedOn(FastLED.leds(), FastLED.size(), BKG);
    }
    //if we're turned off all LEDs, reset the static variables for next time
    if (allOff) {
        led = 0;
        xOffNow = 0;
        szOffNow = turnOffSeq[xOffNow];
        setOff = false;
        return true;
    }

    return false;
}

/**
 * Turns off entire strip by leveraging the shift right with black feed
 * @return true if all leds are off, false otherwise
 */
bool turnOffWipe(bool rightDir) {
    bool allOff = false;
    EVERY_N_MILLISECONDS(60) {
        CRGBSet strip(leds, NUM_PIXELS);
        if (rightDir)
            shiftRight(strip, BKG);
        else
            shiftLeft(strip, BKG);
        FastLED.show(stripBrightness);
        allOff = !isAnyLedOn(leds, NUM_PIXELS, BKG);
    }

    return allOff;
}

/**
 * Adjust strip overall brightness according with the time of day - as follows:
 * <p>Up until 8pm use the max brightness - i.e. <code>BRIGHTNESS</code></p>
 * <p>Between 8pm-9pm - reduce to 80% of full brightness, i.e. scale with 204</p>
 * <p>Between 9-10pm - reduce to 60% of full brightness, i.e. scale with 152</p>
 * <p>After 10pm - reduce to 40% of full brightness, i.e. scale with 102</p>
 */
uint8_t adjustStripBrightness() {
    if (!(stripBrightnessLocked || timeStatus() == timeNotSet)) {
        int hr = hour();
        fract8 scale;
        if (hr < 8)
            scale = 100;
        else if (hr < 20)
            scale = 0;
        else if (hr < 21)
            scale = 204;
        else if (hr < 22)
            scale = 152;
        else
            scale = 102;
        if (scale > 0)
            return dim8_raw(scale8(FastLED.getBrightness(), scale));
    }
    return FastLED.getBrightness();
}

/**
 *
 * @param rgb
 * @param br
 * @return
 */
CRGB& setBrightness(CRGB &rgb, uint8_t br) {
    CHSV hsv = toHSV(rgb);
    hsv.val = br;
    rgb = toRGB(hsv);
    return rgb;
}

/**
 * Approximate brightness - leverages <code>rgb2hsv_approximate</code>
 * @param rgb color
 * @return approximate brightness on HSV scale
 */
uint8_t getBrightness(const CRGB &rgb) {
    return toHSV(rgb).val;
}

// EffectRegistry
LedEffect *EffectRegistry::getCurrentEffect() const {
    return effects[currentEffect];
}

uint16_t EffectRegistry::nextEffectPos(uint16_t efx) {
    uint16_t prevPos = currentEffect;
    currentEffect = capu(efx, effectsCount-1);
    return prevPos;
}

uint16_t EffectRegistry::nextEffectPos() {
    if (!autoSwitch)
        return currentEffect;
    uint16_t prevPos = currentEffect;
    currentEffect = inc(currentEffect, 1, effectsCount);
    return prevPos;
}

uint16_t EffectRegistry::curEffectPos() const {
    return currentEffect;
}

uint16_t EffectRegistry::nextRandomEffectPos() {
    currentEffect = autoSwitch ? random16(0, effectsCount) : currentEffect;
    return currentEffect;
}

uint16_t EffectRegistry::registerEffect(LedEffect *effect) {
    if (effectsCount < MAX_EFFECTS_COUNT) {
        uint8_t curIndex = effectsCount++;
        effects[curIndex] = effect;
        Log.infoln(F("Effect [%s] registered successfully at index %d"), effect->name(), curIndex);
        return curIndex;
    }

    Log.errorln(F("Effects array is FULL, no more effects accepted - this effect NOT registered [%s]"), effect->name());
    return MAX_EFFECTS_COUNT;
}

void EffectRegistry::setup() {
    for (uint16_t x = 0; x < effectsCount; x++) {
        effects[x]->setup();
    }
}

void EffectRegistry::loop() {
    //if effect has changed, re-run the effect's setup
    if (lastEffectRun != currentEffect) {
        Log.infoln(F("Effect change: from index %d [%s] to %d [%s]"),
                lastEffectRun, effects[lastEffectRun]->description(), currentEffect, effects[currentEffect]->description());
        effects[currentEffect]->setup();
        lastEffectRun = currentEffect;
    }
    effects[currentEffect]->loop();
}

void EffectRegistry::describeConfig(JsonArray &json) {
    for (uint16_t x = 0; x < effectsCount; x++) {
        effects[x]->describeConfig(json);
    }
}

LedEffect *EffectRegistry::getEffect(uint16_t index) const {
    return effects[capu(index, effectsCount-1)];
}

void EffectRegistry::autoRoll(bool switchType) {
    autoSwitch = switchType;
}

bool EffectRegistry::isAutoRoll() const {
    return autoSwitch;
}

uint16_t EffectRegistry::size() const {
    return effectsCount;
}

// LedEffect
uint16_t LedEffect::getRegistryIndex() const {
    return registryIndex;
}

void LedEffect::baseConfig(JsonObject &json) const {
    json["description"] = description();
    json["name"] = name();
    json["registryIndex"] = getRegistryIndex();
}

LedEffect::LedEffect(const char *description) : desc(description) {
    //copy into id field the prefix of description (e.g. for a description 'FxA1: awesome lights', copies 'FxA1' into id)
    for (uint8_t i=0; i<LED_EFFECT_ID_SIZE; i++) {
        if (description[i] == ':' || description[i] == '\0') {
            id[i] = '\0';
            break;
        }
        id[i] = description[i];
    }
    //in case the prefix in description is larger, ensure id is null terminated (i.e. truncate)
    id[LED_EFFECT_ID_SIZE-1] = '\0';
    //register the effect
    registryIndex = fxRegistry.registerEffect(this);
}

JsonObject &LedEffect::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
    return obj;
}

const char *LedEffect::name() const {
    return id;
}

const char *LedEffect::description() const {
    return desc;
}

// Viewport
Viewport::Viewport(uint16_t high) : Viewport(0, high) {}

Viewport::Viewport(uint16_t low, uint16_t high) : low(low), high(high) {}

uint16_t Viewport::size() const {
    return qsuba(high, low);
}

//Setup all effects -------------------
void fx_setup() {
    ledStripInit();
    random16_set_seed(millis() >> 2);

    //instantiate effect categories
    for (auto x : categorySetup)
        x();
    //initialize the effects configured in the functions above
    fxRegistry.setup();
    //ensure the current effect is set up, in case they share global variables
    fxRegistry.getCurrentEffect()->setup();

    readState();
}

//Run currently selected effect -------
void fx_run() {
    EVERY_N_SECONDS(30) {
        if (fxBump) {
            fxRegistry.nextEffectPos();
            fxBump = false;
            totalAudioBumps++;
        }
        float msmt = controllerVoltage();
        if (msmt < minVcc)
            minVcc = msmt;
        if (msmt > maxVcc)
            maxVcc = msmt;
#ifndef DISABLE_LOGGING
        Log.infoln(F("Board Vcc voltage %D V"), msmt);
        Log.infoln(F("Chip internal temperature %D 'C"), chipTemperature());
#endif
        msmt = boardTemperature();
        if (msmt < minTemp)
            minTemp = msmt;
        if (msmt > maxTemp)
            maxTemp = msmt;
    }
    EVERY_N_MINUTES(5) {
        fxRegistry.nextRandomEffectPos();
        shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
        stripBrightness = adjustStripBrightness();
        saveState();
        //TODO: revisit these
        countAudioThreshold = 0;
        maxAudio = 0;
    }

    fxRegistry.loop();
    yield();
}
