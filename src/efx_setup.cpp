#include "efx_setup.h"
#include "log.h"
// increase the amount of space for file system to 128kB (default 64kB)
#define RP2040_FS_SIZE_KB   (128)
#include <LittleFSWrapper.h>
#include "TimeLib.h"

//~ Global variables definition
const uint8_t dimmed = 20;
const uint16_t FRAME_SIZE = 50;
const uint8_t flameBrightness = 180;
const uint8_t sparkBrightness = 255;
const CRGB BKG = CRGB::Black;
const uint8_t maxChanges = 24;
volatile bool fxBump = false;
volatile uint16_t speed = 100;
volatile uint16_t curPos = 0;

EffectRegistry fxRegistry;
CRGB leds[NUM_PIXELS];
CRGBArray<NUM_PIXELS> frame;
CRGBPalette16 palette;
CRGBPalette16 targetPalette;
TBlendType    currentBlending;
OpMode mode = Chase;
uint8_t brightness = 224;
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
int8_t rot = 1;
int32_t dist = 1;
bool dirFwd = true;
bool randhue = true;

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

LittleFSWrapper *fsPtr;
const char stateFileName[] = LITTLEFS_FILE_PREFIX "/state.json";

void fsInit() {
#ifndef DISABLE_LOGGING
    mbed::BlockDevice *bd = mbed::BlockDevice::get_default_instance();
    bd->init();
    Log.infoln(F("Default BlockDevice type %s, size %u B, read size %u B, program size %u B, erase size %u B"),
               bd->get_type(), bd->size(), bd->get_read_size(), bd->get_program_size(), bd->get_erase_size());
    bd->deinit();
#endif

    fsPtr = new LittleFSWrapper();
    if (fsPtr->init())
        Log.infoln("Filesystem OK");

#ifndef DISABLE_LOGGING
    Log.infoln(F("Root FS %s contents:"), LittleFSWrapper::getRoot());
    DIR *d = opendir(LittleFSWrapper::getRoot());
    struct dirent *e = nullptr;
    while (e = readdir(d))
        Log.infoln(F("  %s [%d]"), e->d_name, e->d_type);
    Log.infoln(F("Dir complete."));
#endif
}

#define FILE_BUF_SIZE   256
#define JSON_DOC_SIZE   512

/**
 * Reads a text file - if it exists - into a string object
 * @param fname name of the file to read
 * @param s string to store contents into
 * @return number of characters in the file; 0 if file is empty or doesn't exist
 */
size_t readTextFile(const char *fname, String *s) {
    FILE *f = fopen(fname, "r");
    size_t fsize = 0;
    if (f) {
        char buf[FILE_BUF_SIZE];
        memset(buf, 0, FILE_BUF_SIZE);
        size_t cread = 1;
        while (cread = fread(buf, 1, FILE_BUF_SIZE, f)) {
            s->concat(buf, cread);
            fsize += cread;
        }
        fclose(f);
        Log.infoln(F("Read %d bytes from %s file"), fsize, fname);
#ifndef DISABLE_LOGGING
        Log.traceln(F("File %s content [%d]: %s"), fname, fsize, s->c_str());
#endif
    } else
        Log.errorln(F("Text file %s was not found/could not read"), fname);
    return fsize;
}

/**
 * Writes (overrides if already exists) a file using the string content
 * @param fname file name to write
 * @param s contents to write
 * @return number of bytes written
 */
size_t writeTextFile(const char *fname, String *s) {
    size_t fsize = 0;
    FILE *f = fopen(fname, "w");
    if (f) {
        fsize = fwrite(s->c_str(), sizeof(s->charAt(0)), s->length(), f);
        fclose(f);
        Log.infoln(F("File %s has been saved, size %d bytes"), fname, s->length());
    } else
        Log.errorln(F("Failed to create file %s for writing"), fname);
    return fsize;
}

bool removeFile(const char *fname) {
    FILE *f = fopen(fname, "r");
    if (f) {
        fclose(f);
        return lfs.remove(fname) == 0;
    }
    //file does not exist - return true to the caller, the intent is already fulfilled
    return true;
}

void readState() {
    String json;
    size_t stateSize = readTextFile(stateFileName, &json);
    if (stateSize > 0) {
        StaticJsonDocument<JSON_DOC_SIZE> doc; //this takes memory from the thread stack, ensure fx thread's memory size is adjusted if this value is
        deserializeJson(doc, json);

        bool autoAdvance = doc["autoFxRoll"].as<bool>();
        fxRegistry.autoRoll(autoAdvance);

        uint16_t seed = doc["randomSeed"].as<uint16_t>();
        random16_set_seed(seed);

        uint16_t fx = doc["curFx"].as<uint16_t>();
        fxRegistry.nextEffectPos(fx);

        Log.infoln(F("System state restored from %s [%d bytes]: autoFx=%s, randomSeed=%d, nextEffect=%d"), stateFileName, stateSize,
                   autoAdvance ? "true" : "false", seed, fx);
    }
}

void saveState() {
    StaticJsonDocument<JSON_DOC_SIZE> doc;    //this takes memory from the thread stack, ensure fx thread's memory size is adjusted if this value is
    doc["randomSeed"] = random16_get_seed();
    doc["autoFxRoll"] = fxRegistry.isAutoRoll();
    doc["curFx"] = fxRegistry.curEffectPos();
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
    currentBlending = LINEARBLEND;
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

bool isAnyLedOn(CRGB arr[], uint16_t szArray, CRGB backg) {
    for (uint x = 0; x < szArray; x++) {
        if (arr[x] != backg)
            return true;
    }
    return false;
}

void fillArray(const CRGB *src, uint16_t srcLength, CRGB *array, uint16_t arrLength, uint16_t arrOfs) {
    size_t curFrameIndex = arrOfs;
    while (curFrameIndex < arrLength) {
        size_t len = capu(curFrameIndex + srcLength, arrLength) - curFrameIndex;
        copyArray(src, 0, array, curFrameIndex, len);
        curFrameIndex += srcLength;
    }
}

CRGB *mirrorLow(CRGB array[], uint16_t szArray) {
    uint swaps = szArray >> 1;
    for (int x = 0; x < swaps; x++) {
        array[szArray - x - 1] = array[x];
    }
    return array;
}

CRGB *mirrorHigh(CRGB array[], uint16_t szArray) {
    uint swaps = szArray >> 1;
    for (uint x = 0; x < swaps; x++) {
        array[x] = array[szArray - x - 1];
    }
    return array;
}

/**
 * Turns off entire strip by random spots, in increasing size until all leds are off
 * <p>Implemented with timers, no delay - that is why this function needs called repeatedly until it returns true</p>
 * @return true if all leds are off, false otherwise
 */
bool turnOff() {
    static uint16_t led = 0;
    static uint16_t xOffNow = 0;
    static uint16_t szOffNow = turnOffSeq[xOffNow];
    static bool setOff = false;
    bool allOff = false;

    EVERY_N_MILLISECONDS(25) {
        uint8_t ledsOn = 0;
        for (uint16_t x = 0; x < szOffNow; x++) {
            uint16_t xled = stripShuffleIndex[(led + x) % NUM_PIXELS];
            FastLED.leds()[xled].fadeToBlackBy(18);
            if (FastLED.leds()[xled].getLuma() < 4)
                FastLED.leds()[xled] = BKG;
            else
                ledsOn++;
        }
        FastLED.show();
        setOff = ledsOn == 0;
    }

    EVERY_N_MILLISECONDS(700) {
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
 * Turns off entire strip by leveraging the juggle effect
 * @return true if all leds are off, false otherwise
 */
bool turnOffJuggle() {
    bool allOff = false;
    EVERY_N_MILLISECONDS(50) {
        uint numDots = 4;
        uint dotBpm = 7;
        for (uint i = 0; i < numDots; i++) {
            leds[beatsin16(dotBpm + i + numDots, 0, NUM_PIXELS-1)].fadeToBlackBy(192);
        }
        FastLED.show();
        allOff = !isAnyLedOn(leds, NUM_PIXELS, BKG);
    }
    if (allOff) {
        allOff = false;
        return true;
    }
    return false;
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
    EVERY_N_SECONDS(10) {
        if (fxBump) {
            fxRegistry.nextEffectPos();
            fxBump = false;
        }
    }
    EVERY_N_MINUTES(5) {
        fxRegistry.nextRandomEffectPos();
        shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
        saveState();
    }

    fxRegistry.loop();
    yield();
}

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

// LedEffect
uint16_t LedEffect::getRegistryIndex() const {
    return registryIndex;
}

void LedEffect::baseConfig(JsonObject &json) const {
    json["description"] = description();
    json["name"] = name();
    json["registryIndex"] = getRegistryIndex();
}

Viewport::Viewport(uint16_t high) : Viewport(0, high) {}

Viewport::Viewport(uint16_t low, uint16_t high) : low(low), high(high) {}

uint16_t Viewport::size() const {
    return qsuba(high, low);
}
