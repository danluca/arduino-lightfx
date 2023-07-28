#include "efx_setup.h"
#include "log.h"

//~ Global variables definition
CRGB leds[NUM_PIXELS];
EffectRegistry fxRegistry;

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

//~ General Utilities ---------------------------------------------------------
/**
 * Shifts the content of an array to the right by the number of positions specified
 * First item of the array (arr[0]) is used as seed to fill the new elements entering left
 * @param arr array
 * @param szArr size of the array
 * @param pos how many positions to shift right
 */
void shiftRight(CRGB arr[], uint szArr, uint pos) {
    if (pos == 0)
        return;
    CRGB seed = arr[0];
    if (pos >= szArr) {
        fill_solid(arr, (int)szArr, seed);
        return;
    }
    for (int x = szArr-1; x >= 0; x--) {
        arr[x] = x < (int)pos ? seed : arr[x - pos];
    }
}
/**
 * Shifts the content of an array to the left by the number of positions specified
 * The elements entering right are filled with current's array last value (arr[szArr-1])
 * @param arr array
 * @param szArr size of the array
 * @param pos how many positions to shift left
 */
void shiftLeft(CRGB arr[], uint szArr, uint pos) {
    if (pos == 0)
        return;
    CRGB seed = arr[szArr - 1];
    if (pos >= szArr) {
        fill_solid(arr, (int)szArr, seed);
        return;
    }
    for (uint x = 0; x < szArr; x++)
        arr[x] = (x+pos) < szArr ? arr[x+pos] : seed;
}
/**
 * Populates and shuffles randomly an array of numbers in the range of 0-szArray (array indexes)
 * @param array array of indexes
 * @param szArray size of the array
 */
void shuffleIndexes(uint array[], uint szArray) {
    //populates the indexes in ascending order
    for (uint x = 0; x < szArray; x++) {
        array[x] = x;
    }
    //shuffle indexes
    uint swIter = (szArray >> 1) + (szArray >> 3);
    for (uint x = 0; x < swIter; x++) {
        uint r = random16(0, szArray);
        uint tmp = array[x];
        array[x] = array[r];
        array[r] = tmp;
    }
}

void offTrailColor(CRGB arr[], uint x) {
    if (x < 1) {
        arr[x] = CRGB::Black;
        return;
    }
    arr[x] = arr[x - 1];
    arr[x - 1] = CRGB::Black;
}

void setTrailColor(CRGB arr[], uint x, CRGB color, uint8_t dotBrightness, uint8_t trailBrightness) {
    arr[x] = color;
    arr[x] %= dotBrightness;
    if (x < 1) {
        return;
    }
    arr[x - 1] = color.nscale8_video(trailBrightness);
}

//copy arrays using memcpy (arguably fastest way) - no checks are made on the length copied vs actual length of both arrays
void copyArray(CRGB *src, CRGB *dest, size_t length) {
    memcpy(dest, src, sizeof(src[0]) * length);
}

// copy arrays using pointer loops - one of the faster ways. No checks are made on the validity of offsets, length for both arrays
void copyArray(const CRGB *src, uint srcOfs, CRGB *dest, uint destOfs, size_t length) {
    const CRGB *srSt = src + srcOfs;
    CRGB *dsSt = &dest[destOfs];
    for (uint x = 0; x < length; x++) {
        *dsSt++ = *srSt++;
    }
}

bool isAnyLedOn(CRGB arr[], uint szArray, CRGB backg) {
    for (uint x = 0; x < szArray; x++) {
        if (arr[x] != backg)
            return true;
    }
    return false;
}

void fillArray(CRGB *src, size_t szSrc, CRGB color) {
    fill_solid(src, (int)szSrc, color);
}

void fillArray(const CRGB *src, size_t srcLength, CRGB *array, size_t arrLength, uint arrOfs) {
    size_t curFrameIndex = arrOfs;
    while (curFrameIndex < arrLength) {
        size_t len = capu(curFrameIndex + srcLength, arrLength) - curFrameIndex;
        copyArray(src, 0, array, curFrameIndex, len);
        curFrameIndex += srcLength;
    }
}

void showFill(const CRGB frame[], uint szFrame) {
    fillArray(frame, szFrame, FastLED.leds(), FastLED.size());
    FastLED.show();
}

void pushFrame(const CRGB frame[], uint szFrame, uint ofsDest, bool repeat) {
    CRGB *strip = FastLED.leds();
    int szStrip = FastLED.size();
    if (repeat)
        fillArray(frame, szFrame, strip, szStrip, ofsDest);
    else {
        size_t len = capd(ofsDest + szFrame, szStrip) - ofsDest;
        copyArray(frame, 0, strip, ofsDest, len);
    }
}

CRGB *mirrorLow(CRGB array[], uint szArray) {
    uint swaps = szArray >> 1;
    for (int x = 0; x < swaps; x++) {
        array[szArray - x - 1] = array[x];
    }
    return array;
}

CRGB *mirrorHigh(CRGB array[], uint szArray) {
    uint swaps = szArray >> 1;
    for (uint x = 0; x < swaps; x++) {
        array[x] = array[szArray - x - 1];
    }
    return array;
}

CRGB *reverseArray(CRGB array[], uint szArray) {
    uint swaps = szArray >> 1;
    for (int x = 0; x < swaps; x++) {
        CRGB tmp = array[x];
        array[x] = array[szArray - x - 1];
        array[szArray - x - 1] = tmp;
    }
    return array;
}

CRGB *cloneArray(const CRGB src[], CRGB dest[], size_t length) {
    copyArray(src, 0, dest, 0, length);
    return dest;
}

bool turnOffJuggle() {
    static bool allOff = false;
    EVERY_N_MILLISECONDS(50) {
        uint numDots = 4;
        uint dotBpm = 10;
        for (uint i = 0; i < numDots; i++) {
            leds[beatsin16(dotBpm + i + numDots, 0, NUM_PIXELS-1)].fadeToBlackBy(224);
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
    EVERY_N_MINUTES(5) {
        fxRegistry.nextRandomEffectPos();
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

