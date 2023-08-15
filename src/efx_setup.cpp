#include "efx_setup.h"
#include "log.h"
// increase the amount of space for file system to 128kB (default 64kB)
#define RP2040_FS_SIZE_KB   (128)
#include <LittleFSWrapper.h>
#include "TimeLib.h"

//~ Global variables definition
CRGB leds[NUM_PIXELS];
EffectRegistry fxRegistry;
volatile bool fxBump = false;

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
char stateFileName[] = LITTLEFS_FILE_PREFIX "/state.json";

void fsInit() {
#ifndef DISABLE_LOGGING
    mbed::BlockDevice *bd = mbed::BlockDevice::get_default_instance();
    bd->init();
    Log.infoln(F("Default BlockDevice type %s, size %u B, read size %u B, program size %u B, erase size %u B"),
               bd->get_type(), bd->size(), bd->get_read_size(), bd->get_program_size(), bd->get_erase_size());
    bd->deinit();
#endif

    fsPtr = new LittleFSWrapper();
    if (fsPtr->init()) {
        Log.infoln("Successfully mounted the file system");
    } else
        Log.errorln("Could not mount the file system...");
}

void saveState() {
    FILE *f = fopen(stateFileName, "r+");
    if (!f) {
        f = fopen(stateFileName, "w+");
        if (f)
            Log.infoln("Successfully created lastExec file");
        else
            Log.errorln("Failed to create lastExec file");
    } else {
        Log.infoln("Successfully opened existing lastExec file");
        char buff[512];
        memset(buff, 0, 512);
        size_t charsRead = 1;
        Log.infoln("Contents of lastExec file:");
        while (charsRead) {
            charsRead = fread((uint8_t *)buff, 1, 512, f);
            if (charsRead)
                Log.info("%s", buff);
        }
    }
    if (f) {
        //update file
        fprintf(f, "lastRun=%4d-%02d-%02d %02d:%02d:%02d\n", year(), month(), day(), hour(), minute(), second());
        int err = fclose(f);
        if (err < 0)
            Log.errorln("Error saving the lastExec file: %d", err);
        else
            Log.infoln("Successfully saved the lastExec file (%d)", err);
    }

    if (fsPtr) {
        bool err = fsPtr->unmount();
        if (!err)
            Log.errorln("Failed to unmount the file system %d", err);
    }
}

//~ General Utilities ---------------------------------------------------------
/**
 * Shifts the content of an array to the right by the number of positions specified
 * First item of the array (arr[0]) is used as seed to fill the new elements entering left
 * @param arr array
 * @param szArr size of the array
 * @param vwp limits of the shifting area
 * @param pos how many positions to shift right
 * @param feedLeft the color to introduce from the left as we shift the array
 */
void shiftRight(CRGB arr[], uint16_t szArr, Viewport vwp, uint16_t pos, CRGB feedLeft) {
    if ((pos == 0) || (vwp.size() == 0))
        return;
    CRGB seed = feedLeft;
    uint16_t hiArr = capu(vwp.high, szArr);
    if (pos >= vwp.size()) {
        fill_solid(arr + vwp.low, vwp.size(), seed);
        return;
    }
    for (uint16_t x = hiArr; x > vwp.low; x--) {
        uint16_t y = x-1;
        arr[y] = y < pos ? seed : arr[y - pos];
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
void shiftLeft(CRGB arr[], uint16_t szArr, Viewport vwp, uint16_t pos, CRGB feedRight) {
    if ((pos == 0) || (vwp.size() == 0))
        return;
    CRGB seed = feedRight;
    uint16_t hiArr = capu(vwp.high, szArr);
    if (pos >= vwp.size()) {
        fill_solid(arr + vwp.low, hiArr, seed);
        return;
    }
    for (uint16_t x = vwp.low; x < hiArr; x++) {
        uint16_t y = x + pos;
        arr[x] = y < szArr ? arr[y] : seed;
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

void offTrailColor(CRGB arr[], uint16_t x) {
    if (x < 1) {
        arr[x] = CRGB::Black;
        return;
    }
    arr[x] = arr[x - 1];
    arr[x - 1] = CRGB::Black;
}

void setTrailColor(CRGB arr[], uint16_t x, CRGB color, uint8_t dotBrightness, uint8_t trailBrightness) {
    arr[x] = color;
    arr[x] %= dotBrightness;
    if (x < 1) {
        return;
    }
    arr[x - 1] = color.nscale8_video(trailBrightness);
}

//copy arrays using memcpy (arguably the fastest way) - no checks are made on the length copied vs actual length of both arrays
void copyArray(CRGB *src, CRGB *dest, uint16_t length) {
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

void fillArray(CRGB *src, uint16_t szSrc, CRGB color) {
    fill_solid(src, szSrc, color);
}

void fillArray(const CRGB *src, uint16_t srcLength, CRGB *array, uint16_t arrLength, uint16_t arrOfs) {
    size_t curFrameIndex = arrOfs;
    while (curFrameIndex < arrLength) {
        size_t len = capu(curFrameIndex + srcLength, arrLength) - curFrameIndex;
        copyArray(src, 0, array, curFrameIndex, len);
        curFrameIndex += srcLength;
    }
}

void showFill(const CRGB frame[], uint16_t szFrame) {
    fillArray(frame, szFrame, FastLED.leds(), FastLED.size());
    FastLED.show();
}

void pushFrame(const CRGB frame[], uint16_t szFrame, uint16_t ofsDest, bool repeat) {
    CRGB *strip = FastLED.leds();
    int szStrip = FastLED.size();
    if (repeat)
        fillArray(frame, szFrame, strip, szStrip, ofsDest);
    else {
        size_t len = capd(ofsDest + szFrame, szStrip) - ofsDest;
        copyArray(frame, 0, strip, ofsDest, len);
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

CRGB *reverseArray(CRGB array[], uint16_t szArray) {
    uint swaps = szArray >> 1;
    for (int x = 0; x < swaps; x++) {
        CRGB tmp = array[x];
        array[x] = array[szArray - x - 1];
        array[szArray - x - 1] = tmp;
    }
    return array;
}

CRGB *cloneArray(const CRGB src[], CRGB dest[], uint16_t length) {
    copyArray(src, 0, dest, 0, length);
    return dest;
}

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
            FastLED.leds()[xled].fadeToBlackBy(12);
            if (FastLED.leds()[xled].getLuma() < 4)
                FastLED.leds()[xled] = BKG;
            else
                ledsOn++;
        }
        FastLED.show();
        setOff = ledsOn == 0;
    }

    EVERY_N_MILLISECONDS(1200) {
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
    }

    fxRegistry.loop();
    yield();
}

LedEffect *EffectRegistry::getCurrentEffect() const {
    return effects[currentEffect];
}

uint EffectRegistry::nextEffectPos(uint efx) {
    uint prevPos = currentEffect;
    currentEffect = capu(efx, effectsCount-1);
    return prevPos;
}

uint EffectRegistry::nextEffectPos() {
    if (!autoSwitch)
        return currentEffect;
    uint prevPos = currentEffect;
    currentEffect = inc(currentEffect, 1, effectsCount);
    return prevPos;
}

uint EffectRegistry::curEffectPos() const {
    return currentEffect;
}

uint EffectRegistry::nextRandomEffectPos() {
    currentEffect = autoSwitch ? random16(0, effectsCount) : currentEffect;
    return currentEffect;
}

uint EffectRegistry::registerEffect(LedEffect *effect) {
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
    for (uint x = 0; x < effectsCount; x++) {
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
    for (uint x = 0; x < effectsCount; x++) {
        effects[x]->describeConfig(json);
    }
}

LedEffect *EffectRegistry::getEffect(uint index) const {
    return effects[capu(index, effectsCount-1)];
}

void EffectRegistry::autoRoll(bool switchType) {
    autoSwitch = switchType;
}

bool EffectRegistry::isAutoRoll() const {
    return autoSwitch;
}

// LedEffect
uint LedEffect::getRegistryIndex() const {
    return registryIndex;
}

void LedEffect::baseConfig(JsonObject &json) {
    json["description"] = description();
    json["name"] = name();
    json["registryIndex"] = getRegistryIndex();
}

Viewport::Viewport(uint16_t high) : Viewport(0, high) {}

Viewport::Viewport(uint16_t low, uint16_t high) : low(low), high(high) {}

uint16_t Viewport::size() const {
    return qsuba(high, low);
}
