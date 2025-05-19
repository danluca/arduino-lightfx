//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved
//
#include "efx_setup.h"
#include "sysinfo.h"
#include "filesystem.h"
#include "FxSchedule.h"
#include "transition.h"
#include "util.h"

//~ Global variables definition
using namespace fx;
constexpr setupFunc categorySetup[] = {FxA::fxRegister, FxB::fxRegister, FxC::fxRegister, FxD::fxRegister, FxE::fxRegister, FxF::fxRegister, FxH::fxRegister, FxI::fxRegister, FxJ::fxRegister, FxK::fxRegister};
constexpr CRGB BKG = CRGB::Black;

volatile bool fxBump = false;
volatile uint16_t speed = 100;
volatile uint16_t curPos = 0;

EffectRegistry fxRegistry;
CRGB leds[NUM_PIXELS];                                    //the main LEDs array of CRGB type
CRGBSet ledSet(leds, NUM_PIXELS);                     //the entire leds CRGB array as a CRGBSet
CRGBSet tpl(leds, FRAME_SIZE);                        //array length, indexes go from 0 to length-1
CRGBSet others(leds, tpl.size(), NUM_PIXELS-1);  //start and end indexes are inclusive
CRGBArray<PIXEL_BUFFER_SPACE> frame;                      //side LED buffer for preparing/saving state/etc. with main LEDs array
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
uint16_t stripShuffleIndex[NUM_PIXELS];
uint16_t hueDiff = 256;
uint16_t totalAudioBumps = 0;
int32_t dist = 1;
bool stripBrightnessLocked = false;
bool dirFwd = true;
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

void readFxState() {
    const auto json = new String();
    json->reserve(256);  // approximation - currently at 150 bytes
    if (const size_t stateSize = SyncFsImpl.readFile(stateFileName, json); stateSize > 0) {
        JsonDocument doc;
        deserializeJson(doc, *json);

        const bool autoAdvance = doc[csAutoFxRoll].as<bool>();
        fxRegistry.autoRoll(autoAdvance);

        const uint16_t seed = doc[csRandomSeed];
        random16_add_entropy(seed);

        const uint16_t fx = doc[csCurFx];

        stripBrightness = doc[csStripBrightness].as<uint8_t>();

        audioBumpThreshold = doc[csAudioThreshold].as<uint16_t>();
        const auto savedHoliday = doc[csColorTheme].as<String>();
        paletteFactory.setHoliday(parseHoliday(&savedHoliday));
        paletteFactory.setAuto(doc[csAutoColorAdjust].as<bool>());
        if (doc[csSleepEnabled].is<bool>())
            fxRegistry.enableSleep(doc[csSleepEnabled].as<bool>());
        else
            fxRegistry.enableSleep(false);      //this doesn't invoke effect changing because sleep state is initialized with false
        //we need the sleep mode flag setup first to properly advance to next effect
        if (const uint16_t sleepFxIndex = fxRegistry.findEffect(FX_SLEEPLIGHT_ID)->getRegistryIndex(); fx == sleepFxIndex && !fxRegistry.isAsleep())
            fxRegistry.lastEffectRun = fxRegistry.currentEffect = random16(fxRegistry.effectsCount);
        else
            fxRegistry.lastEffectRun = fxRegistry.currentEffect = fx;
        if (doc[csBroadcast].is<bool>())
            fxBroadcastEnabled = doc[csBroadcast].as<bool>();

        log_info(F("System state restored from %s [%zu bytes]: autoFx=%s, randomSeed=%d, nextEffect=%hu, brightness=%hu (auto adjust), audioBumpThreshold=%hu, holiday=%s (auto=%s), sleepEnabled=%s"),
                   stateFileName, stateSize, StringUtils::asString(autoAdvance), seed, fx, stripBrightness, audioBumpThreshold, holidayToString(paletteFactory.getHoliday()), StringUtils::asString(paletteFactory.isAuto()), StringUtils::asString(fxRegistry.isSleepEnabled()));
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
    const auto str = new String();
    str->reserve(measureJson(doc));
    serializeJson(doc, *str);
    if (!SyncFsImpl.writeFile(stateFileName, str))
        log_error(F("Failed to create/write the status file %s"), stateFileName);
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
    const bool flushStrip = sysInfo->isSysStatus(SYS_STATUS_NTP) && !fxRegistry.isAsleep();
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
    hueDiff = 256;
    dist = 1;
    dirFwd = true;
    fxBump = false;

    //shuffle led indexes - when engaging secureRandom functions, each call is about 30ms. Shuffling a 320 items array (~200 swaps and secure random calls) takes about 6 seconds!
    //commented in favor of regular shuffle (every 5 minutes) - see fxRun
    //shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
}

//Setup all effects -------------------
void fx_setup() {
    ledStripInit();
    //instantiate effect categories
    for (const auto x : categorySetup)
        x();
    //strip brightness adjustment needs the time, that's why it is done in fxRun periodically. At the beginning we'll use the value from saved state
    readFxState();
    transEffect.setup();

    shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
    //ensure the current effect is moved to setup state
    fxRegistry.getCurrentEffect()->desiredState(Setup);

    //generate and cache the FX config data
    JsonDocument doc;
    const auto hldList = doc["holidayList"].to<JsonArray>();
    for (uint8_t hi = None; hi <= NewYear; hi++)
        hldList.add(holidayToString(static_cast<Holiday>(hi)));
    const auto fxArray = doc["fx"].to<JsonArray>();
    fxRegistry.describeConfig(fxArray);
    auto *str = new String();
    str->reserve(measureJson(doc));
    serializeJson(doc, *str);
    if (!SyncFsImpl.writeFile(fxCfgFileName, str))
        log_error(F("Cannot save FxConfig JSON file %s"), fxCfgFileName);
    delete str;
    log_info(F("Fx Setup done - current effect %s (%d) set desired state to Setup (%d)"), fxRegistry.getCurrentEffect()->name(),
               fxRegistry.getCurrentEffect()->getRegistryIndex(), Setup);
}

//FW upgrade pattern colors
constexpr CRGB UPGRADE_COLOR_1 = CRGB::IndianRed;
constexpr CRGB UPGRADE_COLOR_2 = CRGB::GreenYellow;
constexpr CRGB UPGRADE_COLOR_3 = CRGB::DarkGray;
constexpr CRGB UPGRADE_COLOR_4 = CRGB::BlueViolet;
constexpr CRGB UPGRADE_COLOR_5 = CRGB::Black;
// Timing constants
constexpr uint8_t BRIGHTNESS_CHECK_INTERVAL_SECONDS = 30;
constexpr uint8_t EFFECT_SWITCH_INTERVAL_MINUTES = 7;

void displayFirmwareUpgradePattern() {
    tpl(0, 4) = UPGRADE_COLOR_1;
    tpl(5, 7) = UPGRADE_COLOR_2;
    tpl[8] = UPGRADE_COLOR_3;
    tpl(9, 11) = UPGRADE_COLOR_2;
    tpl(12, 16) = UPGRADE_COLOR_4;
    tpl(17, tpl.size() - 1) = UPGRADE_COLOR_5;
    replicateSet(tpl, others);
    FastLED.show(stripBrightness);
}

void checkAudioAndBumpEffect() {
    if (fxBump) {
        log_info(F("Audio triggered effect incremental change"));
        fxRegistry.nextEffectPos();
        fxBump = false;
        totalAudioBumps++;
    }
}

void updateBrightness() {
    const uint8_t oldBrightness = stripBrightness;
    stripBrightness = adjustStripBrightness();
    if (oldBrightness != stripBrightness) {
        log_info(F("Strip brightness updated from %d to %d"), oldBrightness, stripBrightness);
    }
}

void switchToRandomEffect() {
    log_info(F("Switching effect to a new random one"));
    fxRegistry.nextRandomEffectPos();
    shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
    saveFxState();
}

//FX Run -------
void fx_run() {
    static bool isFirmwareUpgrading = false;

    if (ulTaskNotifyTake(pdTRUE, 0) == OTA_UPGRADE_NOTIFY) {
        log_info(F("OTA upgrade light pattern"));
        isFirmwareUpgrading = true;
    }

    if (isFirmwareUpgrading) {
        displayFirmwareUpgradePattern();
        watchdogPing();
        return;
    }

    EVERY_N_SECONDS(BRIGHTNESS_CHECK_INTERVAL_SECONDS) {
        checkAudioAndBumpEffect();
        updateBrightness();
    }

    EVERY_N_MINUTES(EFFECT_SWITCH_INTERVAL_MINUTES) {
        switchToRandomEffect();
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
        log_warn(F("Bedtime alarm triggered, sleep mode is disabled - no changes"));
}

