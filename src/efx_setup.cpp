//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
#include "efx_setup.h"
#include "sysinfo.h"
#include "broadcast.h"
#include "filesystem.h"
#include "log.h"

//~ Global variables definition
#define JSON_DOC_SIZE   512
const uint8_t dimmed = 20;
const char csAutoFxRoll[] PROGMEM = "autoFxRoll";
const char csStripBrightness[] PROGMEM = "stripBrightness";
const char csAudioThreshold[] PROGMEM = "audioThreshold";
const char csColorTheme[] PROGMEM = "colorTheme";
const char csAutoColorAdjust[] PROGMEM = "autoColorAdjust";
const char csRandomSeed[] PROGMEM = "randomSeed";
const char csCurFx[] PROGMEM = "curFx";
const char csSleepEnabled[] PROGMEM = "sleepEnabled";
const char csBrightness[] PROGMEM = "brightness";
const char csBrightnessLocked[] PROGMEM = "brightnessLocked";
const char csAuto[] PROGMEM = "auto";
const char csHoliday[] PROGMEM = "holiday";
const char strNR[] PROGMEM = "N/R";
const char csBroadcast[] PROGMEM = "broadcast";
const setupFunc categorySetup[] = {FxA::fxRegister, FxB::fxRegister, FxC::fxRegister, FxD::fxRegister, FxE::fxRegister, FxF::fxRegister, FxH::fxRegister, FxI::fxRegister, FxJ::fxRegister, FxK::fxRegister};

//const uint16_t FRAME_SIZE = 68;     //NOTE: frame size must be at least 3 times less than NUM_PIXELS. The frame CRGBSet must fit at least 3 frames
const CRGB BKG = CRGB::Black;
const uint8_t maxChanges = 24;
volatile bool fxBump = false;
volatile uint16_t speed = 100;
volatile uint16_t curPos = 0;

EffectRegistry fxRegistry;
CRGB leds[NUM_PIXELS];                                    //the main LEDs array of CRGB type
CRGBSet ledSet(leds, NUM_PIXELS);                     //the entire leds CRGB array as a CRGBSet
CRGBSet tpl(leds, FRAME_SIZE);                        //array length, indexes go from 0 to length-1
CRGBSet others(leds, tpl.size(), NUM_PIXELS-1); //start and end indexes are inclusive
CRGBArray<NUM_PIXELS> frame;                              //side LED buffer for preparing/saving state/etc. with main LEDs array
CRGBPalette16 palette;
CRGBPalette16 targetPalette;
OpMode mode = Chase;
uint8_t brightness = 224;
uint8_t stripBrightness = brightness;
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
bool stripBrightnessLocked = false;
bool dirFwd = true;
bool randhue = true;
EffectTransition transEffect;

//~ Support functions -----------------
/**
 * Setup the strip LED lights to be controlled by FastLED library
 */
void ledStripInit() {
    CFastLED::addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_PIXELS).setCorrection(TypicalSMD5050).setTemperature(Tungsten100W);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear(true);
}

void stateLED(CRGB color) {
    updateStateLED(color.r, color.g, color.b);
}

void readFxState() {
    auto json = new String();
    json->reserve(256);  // approximation - currently at 150 bytes
    size_t stateSize = readTextFile(stateFileName, json);
    if (stateSize > 0) {
        JsonDocument doc;
        deserializeJson(doc, *json);

        bool autoAdvance = doc[csAutoFxRoll].as<bool>();
        fxRegistry.autoRoll(autoAdvance);

        uint16_t seed = doc[csRandomSeed];
        random16_add_entropy(seed);

        uint16_t fx = doc[csCurFx];

        stripBrightness = doc[csStripBrightness].as<uint8_t>();

        audioBumpThreshold = doc[csAudioThreshold].as<uint16_t>();
        String savedHoliday = doc[csColorTheme].as<String>();
        paletteFactory.setHoliday(parseHoliday(&savedHoliday));
        paletteFactory.setAuto(doc[csAutoColorAdjust].as<bool>());
        if (doc.containsKey(csSleepEnabled))
            fxRegistry.enableSleep(doc[csSleepEnabled].as<bool>());
        else
            fxRegistry.enableSleep(false);      //this doesn't invoke effect changing because sleep state is initialized with false
        //we need the sleep mode flag setup first to properly advance to next effect
        const uint16_t sleepFxIndex = fxRegistry.findEffect(FX_SLEEPLIGHT_ID)->getRegistryIndex();
        if (fx == sleepFxIndex && !fxRegistry.isAsleep())
            fxRegistry.lastEffectRun = fxRegistry.currentEffect = random16(fxRegistry.effectsCount);
        else
            fxRegistry.lastEffectRun = fxRegistry.currentEffect = fx;
        if (doc[csBroadcast].is<bool>())
            fxBroadcastEnabled = doc[csBroadcast].as<bool>();

        Log.infoln(F("System state restored from %s [%d bytes]: autoFx=%T, randomSeed=%d, nextEffect=%d, brightness=%d (auto adjust), audioBumpThreshold=%d, holiday=%s (auto=%T), sleepEnabled=%T"),
                   stateFileName, stateSize, autoAdvance, seed, fx, stripBrightness, audioBumpThreshold, holidayToString(paletteFactory.getHoliday()), paletteFactory.isAuto(), fxRegistry.isSleepEnabled());
    }
    delete json;
}

void saveFxState() {
    JsonDocument doc;
    doc[csRandomSeed] = random16_get_seed();
    doc[csAutoFxRoll] = fxRegistry.isAutoRoll();
    doc[csCurFx] = fxRegistry.curEffectPos();
    doc[csStripBrightness] = stripBrightness;
    doc[csAudioThreshold] = audioBumpThreshold;
    doc[csColorTheme] = holidayToString(paletteFactory.getHoliday());
    doc[csAutoColorAdjust] = paletteFactory.isAuto();
    doc[csSleepEnabled] = fxRegistry.isSleepEnabled();
    doc[csBroadcast] = fxBroadcastEnabled;
    auto str = new String();
    str->reserve(measureJson(doc));
    serializeJson(doc, *str);
    if (!writeTextFile(stateFileName, str))
        Log.errorln(F("Failed to create/write the status file %s"), stateFileName);
    delete str;
}

//~ General Utilities ---------------------------------------------------------
/**
 * Resets the state of all global variables, including the state of the LED strip. Suitable for a clean slate
 * start of a new Fx
 * <p>This needs to account for ALL global variables</p>
 */
void resetGlobals() {
    //turn off the LEDs on the strip and the frame buffer - flush to the LED strip if we have the time and not in sleep time
    //flushing to strip may cause a short blink if called mid-effect, like an audio effect bump would do for the same effect when sleeping
    bool flushStrip = sysInfo->isSysStatus(SYS_STATUS_NTP) && !fxRegistry.isAsleep();
    FastLED.clear(flushStrip);
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

    //shuffle led indexes - when engaging secureRandom functions, each call is about 30ms. Shuffling a 320 items array (~200 swaps and secure random calls) takes about 6 seconds!
    //commented in favor of regular shuffle (every 5 minutes) - see fxRun
    //shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
}

/**
 * Ease Out Bounce implementation - leverages the double precision original implementation converted to int in a range
 * @param x input value
 * @param lim high limit range
 * @return the result in [0,lim] inclusive range
 * @see https://easings.net/#easeOutBounce
 */
uint16_t easeOutBounce(const uint16_t x, const uint16_t lim) {
    static const float d1 = 2.75f;
    static const float n1 = 7.5625f;

    float xf = ((float)x)/(float)lim;
    float res = 0;
    if (xf < 1/d1) {
        res = n1*xf*xf;
    } else if (xf < 2/d1) {
        float xf1 = xf - 1.5f/d1;
        res = n1*xf1*xf1 + 0.75f;
    } else if (xf < 2.5f/d1) {
        float xf1 = xf - 2.25f/d1;
        res = n1*xf1*xf1 + 0.9375f;
    } else {
        float xf1 = xf - 2.625f/d1;
        res = n1*xf1*xf1 + 0.984375f;
    }
    return (uint16_t )(res * (float)lim);
}

/**
 * Ease Out Quad implementation - leverages the double precision original implementation converted to int in a range
 * @param x input value
 * @param lim high limit range
 * @return the result in [0,lim] inclusive range
 * @see https://easings.net/#easeOutQuad
 */
uint16_t easeOutQuad(const uint16_t x, const uint16_t lim) {
    auto limf = float(lim);
    float xf = float(x)/limf;
    return uint16_t((1 - (1-xf)*(1-xf))*limf);
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
    if (pos >= vwp.size()) {
        uint16_t hiMark = capu(vwp.high, (set.size()-1));
        set(vwp.low, hiMark) = feedLeft;
        return;
    }
    uint16_t hiMark = capu(vwp.high, set.size());
    //don't use >= as the indexer is unsigned and always >=0 --> infinite loop
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
    uint16_t hiMark = capu(vwp.high, (set.size()-1));
    if (pos >= vwp.size()) {
        set(vwp.low, hiMark) = feedRight;
        return;
    }
    for (uint16_t x = vwp.low; x <= hiMark; x++) {
        uint16_t y = x + pos;
        set[x] = y < set.size() ? set[y] : feedRight;
    }
}

/**
 * Spread the color provided into the pixels set starting from the left, in gradient steps
 * Note that while the color is spread, the rest of the pixels in the set remain in place - this is
 * different than shifting the set while feeding the color from one end
 * @param set LED strip to update
 * @param color the color to spread
 * @param gradient amount of gradient to use per each spread call
 * @return true when all pixels in the set are at full spread color value
 */
bool spreadColor(CRGBSet &set, CRGB color, uint8_t gradient) {
    uint16_t clrPos = 0;
    while ((set[clrPos] == color) && clrPos < set.size())
        clrPos++;

    if (clrPos == set.size())
        return true;

    //the nblend takes care about the edge cases of gradient 0 and 255. When 0 it returns the source color unchanged;
    //when 255 it replaces source color entirely with the overlay
    nblend(set[clrPos], color, gradient);
    return false;
}

/**
 * Moves the segment from old position to new position by leveraging blend, for a smooth transition
 * @param target pixel set that receives the move - this is (much) larger in size than the segment
 * @param segment the segment to move, will not be changed - this is (much) smaller in size than the target
 * @param fromPos old position - the index where the segment is currently at in the target pixel set. The index is the next position from the END of the segment.
 *  Ranges from 0 to target.size()+segment.size()
 * @param toPos new position - the index where the segment needs to be moved at in the target pixel set. The index is the next position from the END of the segment.
 *  Ranges from 0 to target.size()+segment.size()
 * @return true when the move is complete, that is when the segment at new position in the target set matches the original segment completely (has blended)
 */
bool moveBlend(CRGBSet &target, const CRGBSet &segment, fract8 overlay, uint16_t fromPos, uint16_t toPos) {
    const uint16_t segSize = abs(segment.len);  //we want segment reference to be constant, but the size() function is not marked const in the library
    const uint16_t tgtSize = target.size();
    const uint16_t maxSize = tgtSize + segSize;
    if (fromPos == toPos || (fromPos >= maxSize && toPos >= maxSize))
        return true;
    bool isOldEndSegmentWithinTarget = fromPos < tgtSize;
    bool isNewEndSegmentWithinTarget = toPos < tgtSize;
    bool isOldStartSegmentWithinTarget = fromPos >= segSize;
    bool isNewStartSegmentWithinTarget = toPos >= segSize;
    CRGB bkg = isOldEndSegmentWithinTarget ? target[fromPos] : target[fromPos - segSize - 1];   //if old pos (pixel after segment end) wasn't within target boundaries, choose the pixel before the segment begin for background
    uint16_t newPosTargetStart = qsuba(toPos, segSize);
    uint16_t newPosTargetEnd = capu(toPos, target.size() - 1);
    if (toPos > fromPos) {
        if (isOldStartSegmentWithinTarget || isNewStartSegmentWithinTarget)
            target(qsuba(fromPos, segSize), qsuba(newPosTargetStart, 1)).nblend(bkg, overlay);
    } else {
        if (isOldEndSegmentWithinTarget || isNewEndSegmentWithinTarget)
            target(capu(fromPos, target.size()-1), qsuba(newPosTargetEnd, 1)).nblend(bkg, overlay);
    }
    uint16_t startIndexSeg = isNewStartSegmentWithinTarget ? 0 : segSize - toPos;
    uint16_t endIndexSeg = isNewEndSegmentWithinTarget ? segSize - 1 : maxSize - toPos - 1;
    CRGBSet sliceTarget((CRGB*)target, newPosTargetStart, newPosTargetEnd);
    CRGBSet sliceSeg((CRGB*)segment, startIndexSeg, endIndexSeg);
    sliceTarget.nblend(sliceSeg, overlay);
    return areSame(sliceTarget, sliceSeg);
}

/**
 * Are contents of the two sets the same - this is different than the == operator (that checks whether they point to the same thing)
 * @param lhs left hand set
 * @param rhs right hand set
 * @return true if same length and same content
 */
bool areSame(const CRGBSet &lhs, const CRGBSet &rhs) {
    if (abs(lhs.len) != abs(rhs.len))
        return false;
    for (uint16_t i = 0; i < (uint16_t)abs(lhs.len); ++i) {
        if (lhs[i] != rhs[i])
            return false;
    }
    return true;
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
        for (auto & y : dest) {
            CRGB* yPtr = &y;
            if ((yPtr < normSrcStart) || (yPtr >= normSrcEnd)) {
                (*yPtr) = src[x];
                incr(x, 1, srcSize);
            }
        }
    } else {
        //no overlap - simpler assignment code
        for (auto & y : dest) {
            y = src[x];
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
    for (uint16_t x = 0; x < szArray; x++)
        array[x] = x;
    //shuffle indexes
    uint16_t swIter = (szArray >> 1) + (szArray >> 3);
    for (uint16_t x = 0; x < swIter; x++) {
        uint16_t r = random16(0, szArray);
        uint16_t tmp = array[x];
        array[x] = array[r];
        array[r] = tmp;
    }
}

void shuffle(CRGBSet &set) {
    //perform a number of swaps with random elements of the array - randomness provided by ECC608 secure random number generator
    uint16_t swIter = (set.size() >> 1) + (set.size() >> 4);
    for (uint16_t x = 0; x < swIter; x++) {
        uint16_t r = random16(0, set.size());
        CRGB tmp = set[x];
        set[x] = set[r];
        set[r] = tmp;
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

uint16_t countPixelsBrighter(CRGBSet *set, CRGB backg) {
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
 * @param blendRGB base color, which is also the target (the one receiving the result)
 * @param topRGB color to multiply with
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
 * Repeatedly blends b into a with a given amount until a=b
 * @param a recipient
 * @param b source
 * @param amt amount of blending
 * @return true when the recipient is the same as source; false otherwise
 */
bool rblend8(uint8_t &a, const uint8_t b, const uint8_t amt) {
    if (a == b || amt == 0)
        return true;
    if (amt == 255) {
        a = b;
        return true;
    }
    if (a < b)
        a += scale8_video(b - a, amt);
    else
        a -= scale8_video(a - b, amt);
    return a == b;
}

/**
 * Blends the target color into an existing (in-place) such that after a number of iterations
 * the existing becomes equal with the target
 * <p>For repeated blending calls, this works better than <code>nblend(CRGB &existing, CRGB &target, uint8 overlay)</code> as it
 * will actually make existing reach the target value</p>
 * @param existing color to modify
 * @param target target color
 * @param frOverlay fraction (number of 256-ths) of the target color to blend into existing. Special meaning to extreme values:
 * 0 is a no-op, existing is not changed; 255 forces the existing to equal to target
 * @return true if the existing color has become equal with target or overlay fraction is 0 (no blending); false otherwise
 */
bool rblend(CRGB &existing, const CRGB &target, const fract8 frOverlay) {
    if (existing == target || frOverlay == 0)
        return true;
    if (frOverlay == 255) {
        existing = target;
        return true;
    }
    bool bRed = rblend8(existing.red, target.red, frOverlay);
    bool bGreen = rblend8(existing.green, target.green, frOverlay);
    bool bBlue = rblend8(existing.blue, target.blue, frOverlay);
    return bRed && bGreen && bBlue;
}


/**
 * Adjust strip overall brightness according with the time of day - as follows:
 * <p>Up until 8pm use the max brightness - i.e. <code>BRIGHTNESS</code></p>
 * <p>Between 8pm-9pm - reduce to 80% of full brightness, i.e. scale with 204</p>
 * <p>Between 9-10pm - reduce to 60% of full brightness, i.e. scale with 152</p>
 * <p>After 10pm - reduce to 40% of full brightness, i.e. scale with 102</p>
 */
uint8_t adjustStripBrightness() {
    if (!stripBrightnessLocked && sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
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
    //compare with rgb.getLuma() ?
    return toHSV(rgb).val;
}

void adjustCurrentEffect(const time_t time) {
    fxRegistry.setSleepState(!isAwakeTime(time));
}

// EffectRegistry
LedEffect *EffectRegistry::getCurrentEffect() const {
    return effects[currentEffect];
}

uint16_t EffectRegistry::nextEffectPos(const char *id) {
    for (size_t x = 0; x < effects.size(); x++) {
        if (strcmp(id, effects[x]->name()) == 0) {
            currentEffect = x;
            transitionEffect();
            return lastEffectRun;
        }
    }
    return 0;
}

uint16_t EffectRegistry::nextEffectPos(uint16_t efx) {
    currentEffect = capu(efx, effectsCount-1);
    transitionEffect();
    return lastEffectRun;
}

uint16_t EffectRegistry::nextEffectPos() {
    if (!autoSwitch || sleepState)
        return currentEffect;
    currentEffect = inc(currentEffect, 1, effectsCount);
    //increment past the sleep effect, if landed on it
    if (currentEffect == sleepEffect)
        currentEffect = inc(currentEffect, 1, effectsCount);
    transitionEffect();
    return lastEffectRun;
}

uint16_t EffectRegistry::curEffectPos() const {
    return currentEffect;
}

uint16_t EffectRegistry::nextRandomEffectPos() {
    if (autoSwitch && !sleepState) {
        //weighted randomization of the next effect index
        uint16_t totalSelectionWeight = 0;
        for (auto const *fx:effects)
            totalSelectionWeight += fx->selectionWeight();  //this allows each effect's weight to vary with time, holiday, etc.
        uint16_t rnd = random16(0, totalSelectionWeight);
        for (uint16_t i = 0; i < effectsCount; ++i) {
            rnd = qsuba(rnd, effects[i]->selectionWeight());
            if (rnd == 0) {
                currentEffect = i;  //sleep effect weight is 0, so it cannot be chosen randomly
                break;
            }
        }
        transitionEffect();
    }
    return currentEffect;
}

void EffectRegistry::transitionEffect() const {
    if (currentEffect != lastEffectRun) {
        effects[lastEffectRun]->desiredState(Idle);
        transEffect.setup();
    }
    effects[currentEffect]->desiredState(Running);
}

uint16_t EffectRegistry::registerEffect(LedEffect *effect) {
    effects.push_back(effect);  //pushing from the back to preserve the order or insertion during iteration
    effectsCount = effects.size();
    uint16_t fxIndex = effectsCount - 1;
    if (strcmp(FX_SLEEPLIGHT_ID, effect->name()) == 0)
        sleepEffect = fxIndex;
    Log.infoln(F("Effect [%s] registered successfully at index %d"), effect->name(), fxIndex);
    return fxIndex;
}

LedEffect *EffectRegistry::findEffect(const char *id) {
    for (auto &fx : effects) {
        if (strcmp(id, fx->name()) == 0)
            return fx;
    }
    return nullptr;
}

void EffectRegistry::setSleepState(const bool sleepFlag) {
    if (sleepState != sleepFlag) {
        sleepState = sleepFlag;
        Log.infoln(F("Switching to sleep state %T (sleep mode enabled %T)"), sleepState, sleepModeEnabled);
        if (sleepState) {
            nextEffectPos(FX_SLEEPLIGHT_ID);
        } else {
            if (lastEffects.size() < 2)
                nextRandomEffectPos();
            else {
                uint16_t prevFx = *(lastEffects.end()-2); //second entry in the queue (from the inserting point - the end) is the previous effect before the sleep mode
                currentEffect = prevFx;
                transitionEffect();
            }
        }
    } else
        Log.infoln(F("Sleep state is already %T - no changes"), sleepState);
}

void EffectRegistry::enableSleep(bool bSleep) {
    sleepModeEnabled = bSleep;
    Log.infoln(F("Sleep mode enabled is now %T"), sleepModeEnabled);
    //determine the proper sleep status based on time
    setSleepState(sleepModeEnabled && !isAwakeTime(now()));
}

/**
 * Sets the desired state for all effects to setup. It will essentially make each effect ready to run if invoked - the state machine will ensure the setup() is called first
 */
void EffectRegistry::setup() {
    for (auto &fx : effects)
        fx->desiredState(Setup);
    transEffect.setup();
}

void EffectRegistry::loop() {
    //if effect has changed, re-run the effect's setup
    if ((lastEffectRun != currentEffect) && (effects[lastEffectRun]->getState() == Idle)) {
        Log.infoln(F("Effect change: from index %d [%s] to %d [%s]"),
                lastEffectRun, effects[lastEffectRun]->description(), currentEffect, effects[currentEffect]->description());
        lastEffectRun = currentEffect;
        lastEffects.push(lastEffectRun);
        postFxChangeEvent(lastEffectRun);
    }
    effects[lastEffectRun]->loop();
}

void EffectRegistry::describeConfig(JsonArray &json) {
    for (auto & effect : effects)
        effect->describeConfig(json);
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

bool EffectRegistry::isSleepEnabled() const {
    return sleepModeEnabled;
}

bool EffectRegistry::isAsleep() const {
    return sleepState;
}

uint16_t EffectRegistry::size() const {
    return effectsCount;
}

void EffectRegistry::pastEffectsRun(JsonArray &json) {
    for (const auto &fxIndex: lastEffects)
        json.add(getEffect(fxIndex)->name());
}

// LedEffect
uint16_t LedEffect::getRegistryIndex() const {
    return registryIndex;
}

void LedEffect::baseConfig(JsonObject &json) const {
    json["description"] = description();
    json["name"] = name();
    json["registryIndex"] = getRegistryIndex();
    json["palette"] = holidayToString(paletteFactory.getHoliday());
}

LedEffect::LedEffect(const char *description) : state(Idle), desc(description) {
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
    JsonObject obj = json.add<JsonObject>();
    baseConfig(obj);
    return obj;
}

const char *LedEffect::name() const {
    return id;
}

const char *LedEffect::description() const {
    return desc;
}

/**
 * Non-repeat by design. All the setup occurs in one blocking step.
 */
void LedEffect::setup() {
    resetGlobals();
}

/**
 * Performs the transition to off - by default a pause of 1 second. Subclasses can override the behavior - function is virtual
 * This is a repeat function - it is called multiple times while in TransitionBreak state, until it returns true.
 * @return true if transition has completed; false otherwise
 */
bool LedEffect::transitionBreak() {
    //if we need to customize the amount of pause between effects, make a global variable (same for all effects) or a local class member (custom for each effect)
    return millis() > (transOffStart + 1000);
}

/**
 * Called only once as the effect transitions into TransitionBreak state, before the loop calls to <code>transitionBreak</code>
 */
void LedEffect::transitionBreakPrep() {
    //nothing for now
}

/**
 * This is a repeat function - it is called multiple times while in WindDown state, until it returns true
 * @return true if transition has completed; false otherwise
 */
bool LedEffect::windDown() {
    return transEffect.transition();
}

/**
 * Called only once as the effect transitions into WindDown state, before the loop calls to <code>windDown</code>
 */
void LedEffect::windDownPrep() {
    CRGBSet strip(leds, NUM_PIXELS);
    strip.nblend(ColorFromPalette(targetPalette, random8(), 72, LINEARBLEND), 80);
    FastLED.show(stripBrightness);
    transEffect.prepare(rot);
}

/**
 * Re-entrant looping function
 */
void LedEffect::loop() {
    switch (state) {
        case Setup:
            setup();
            Log.infoln(F("Effect %s [%d] completed setup, moving to running state"), name(), getRegistryIndex());
            nextState();
            break;    //one blocking step, non repeat
        case Running: run(); break;                 //repeat, called multiple times to achieve the light effects designed
        case WindDownPrep:
            windDownPrep();
            Log.infoln(F("Effect %s [%d] completed WindDown Prep"), name(), getRegistryIndex());
            nextState();
            break;
        case WindDown:
            if (windDown()) {
                Log.infoln(F("Effect %s [%d] completed WindDown"), name(), getRegistryIndex());
                nextState();
            }
            break;           //repeat, called multiple times to achieve the fade out for the current light effect
        case TransitionBreakPrep:
            transitionBreakPrep();
            Log.infoln(F("Effect %s [%d] completed TransitionBreak Prep"), name(), getRegistryIndex());
            nextState();
            break;
        case TransitionBreak:
            if (transitionBreak()) {
                Log.infoln(F("Effect %s [%d] completed TransitionBreak"), name(), getRegistryIndex());
                nextState();
            }
            break; //repeat, called multiple times to achieve the transition off for the current light effect
        case Idle: break;                           //no-op
    }
}

/**
 * Implementation of desired state - informs the state machine of the intended next state and causes it to react.
 * This means either transitioning to an interim state that precedes the desired state, or directly switch to desired state
 * @param dst intended next state
 */
void LedEffect::desiredState(EffectState dst) {
    if (state == dst)
        return;     //already there
    switch (state) {
        case Setup:
            switch (dst) {
                case Idle: state = dst; break;
                case Running:
                case WindDownPrep:
                case WindDown:
                case TransitionBreakPrep:
                case TransitionBreak: return;   //not a valid transition, Setup may not have completed
            }
            break;
        case Running:
            switch (dst) {
                case WindDownPrep:
                case WindDown:
                case TransitionBreakPrep:
                case TransitionBreak:
                case Idle:
                case Setup: state = WindDownPrep; break;
            }
            break;
        case WindDownPrep:
            switch (dst) {
                case WindDown:
                case TransitionBreak:
                case TransitionBreakPrep:
                case Setup:
                case Idle: state = WindDown; break;
                case Running: state = dst; break;
            }
            break;
        case WindDown:
            //any transitions here will cut short the in-progress windDown function
            switch (dst) {
                case TransitionBreakPrep:
                case TransitionBreak:
                case Idle:
                case Setup: state = TransitionBreakPrep; transOffStart = millis(); break;
                case Running: state = dst; break;
                case WindDownPrep: return;  //not a valid transition
            }
            break;
        case TransitionBreakPrep:
            switch (dst) {
                case Idle:
                case Setup: state = Idle; break;
                case Running: state = Setup; break;
                case TransitionBreak: state = dst; break;
                case WindDown:
                case WindDownPrep: return;  //not a valid transition
            }
            break;
        case TransitionBreak:
            //any transitions here will cut short the in-progress transitionOff function
            switch (dst) {
                case Idle:
                case Setup: state = Idle; break;
                case Running: state = Setup; break;
                case TransitionBreakPrep:
                case WindDownPrep:
                case WindDown: return;  //not a valid transition
            }
            break;
        case Idle:
            switch (dst) {
                case Running:
                case Setup: state = Setup; break;
                case WindDownPrep:
                case WindDown:
                case TransitionBreakPrep:
                case TransitionBreak: return;   //not a valid transition
            }
            break;
    }
}

void LedEffect::nextState() {
    switch (state) {
        case Setup: state = Running; break;
        case Running: state = WindDownPrep; break;
        case WindDownPrep: state = WindDown; break;
        case WindDown: state = TransitionBreakPrep; transOffStart = millis(); break;
        case TransitionBreakPrep: state = TransitionBreak; break;
        case TransitionBreak: state = Idle; break;
        case Idle: state = Setup; break;
    }
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
    //instantiate effect categories
    for (auto x : categorySetup)
        x();
    //strip brightness adjustment needs the time, that's why it is done in fxRun periodically. At the beginning we'll use the value from saved state
    readFxState();
    transEffect.setup();

    shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
    //ensure the current effect is moved to setup state
    fxRegistry.getCurrentEffect()->desiredState(Setup);
    Log.infoln("Fx Setup done - current effect %s (%d) set desired state to Setup (%d)", fxRegistry.getCurrentEffect()->name(),
               fxRegistry.getCurrentEffect()->getRegistryIndex(), Setup);
}

//Run currently selected effect -------
void fx_run() {
    EVERY_N_SECONDS(30) {
        if (fxBump) {
            Log.infoln(F("Audio triggered effect incremental change"));
            fxRegistry.nextEffectPos();
            fxBump = false;
            totalAudioBumps++;
        }
        uint8_t oldBrightness = stripBrightness;
        stripBrightness = adjustStripBrightness();
        if (oldBrightness != stripBrightness)
            Log.infoln(F("Strip brightness updated from %d to %d"), oldBrightness, stripBrightness);
    }
    EVERY_N_MINUTES(7) {
        Log.infoln(F("Switching effect to a new random one"));
        fxRegistry.nextRandomEffectPos();
        shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
        saveFxState();
    }

    fxRegistry.loop();
    watchdogPing();
}

// FxSchedule functions
void wakeup() {
//    if (fxRegistry.isSleepEnabled())
    fxRegistry.setSleepState(false);
}

void bedtime() {
    if (fxRegistry.isSleepEnabled())
        fxRegistry.setSleepState(true);
    else
        Log.warningln(F("Bedtime alarm triggered, sleep mode is disabled - no changes"));
}

