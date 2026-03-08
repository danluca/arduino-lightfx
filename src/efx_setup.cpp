//
// Copyright (c) 2023,2024,2025,2026 by Dan Luca. All rights reserved
//
#include "efx_setup.h"
#include "HealthMonitor.h"
#include "sysinfo.h"
#include "filesystem.h"
#include "FxSchedule.h"
#include "transition.h"
#include "util.h"
#include "task_msg.h"
#include "constants.hpp"
#include <hardware/watchdog.h>

//~ Global variables definition
using namespace fx;
const setupFunc categorySetup[] = {FxA::fxRegister, FxB::fxRegister, FxC::fxRegister, FxD::fxRegister, FxE::fxRegister, FxF::fxRegister, FxH::fxRegister, FxI::fxRegister, FxJ::fxRegister, FxK::fxRegister};
constexpr CRGB BKG = CRGB::Black;

//~ Global variables accessed by multiple tasks
QueueHandle_t fxQueue;
EffectRegistry fxRegistry;
std::atomic<uint16_t> speed = 100;
std::atomic<uint16_t> curPos = 0;
volatile uint8_t brightness = 224;
std::atomic<uint8_t> stripBrightness = brightness;
volatile uint8_t colorIndex = 0;
volatile uint8_t lastColorIndex = 0;
volatile uint8_t fade = 8;
volatile uint8_t hue = 50;
volatile uint8_t delta = 1;
volatile uint8_t saturation = 100;
volatile uint8_t dotBpm = 30;
volatile uint16_t hueDiff = 256;
std::atomic<bool> stripBrightnessLocked = false;
mutex_t fxRegistryMutex;

static_assert(FRAME_SIZE < NUM_PIXELS, "FRAME_SIZE must not exceed NUM_PIXELS");
static_assert(FRAME_SIZE > 10, "FRAME_SIZE must be at least 10 pixels");
static_assert(PIXEL_BUFFER_SPACE > FRAME_SIZE * 3, "PIXEL_BUFFER_SPACE must be at least 3 times the FRAME_SIZE");

//~ fx namespace variables (keep these as single-task FX access)
namespace fx {
    CRGB leds[NUM_PIXELS];                                    //the main LEDs array of CRGB type
    CRGBSet ledSet(leds, NUM_PIXELS);                     //the entire leds CRGB array as a CRGBSet
    CRGBSet tpl(leds, FRAME_SIZE);                        //array length, indexes go from 0 to length-1
    CRGBSet others(leds, tpl.size(), NUM_PIXELS-1);  //start and end indexes are inclusive
    CRGBArray<PIXEL_BUFFER_SPACE> frame;                      //side LED buffer for preparing/saving state/etc. with main LEDs array
    CRGBPalette16 palette;
    CRGBPalette16 targetPalette;
    OpMode mode = Chase;
    uint16_t stripShuffleIndex[NUM_PIXELS];
    int32_t dist = 1;
    bool dirFwd = true;
    EffectTransition transEffect;
}

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
        log_info(F("FX state [%s]:\n%s"), stateFileName, json->c_str());

        const bool autoAdvance = doc[csAutoFxRoll].as<bool>();
        {
            CoreMutex lock(&fxRegistryMutex);
            fxRegistry.autoRoll(autoAdvance);
        }

        const uint16_t seed = doc[csRandomSeed].as<uint16_t>();
        random16_add_entropy(seed);

        const uint16_t fx = doc[csCurFx].as<uint16_t>();

        stripBrightness = doc[csStripBrightness].as<uint8_t>();

        const auto savedHoliday = doc[csColorTheme].as<String>();
        paletteFactory.setHoliday(parseHoliday(&savedHoliday));
        paletteFactory.setAuto(doc[csAutoColorAdjust].as<bool>());
        {
            CoreMutex lock(&fxRegistryMutex);
            if (doc[csSleepEnabled].is<bool>())
                fxRegistry.enableSleep(doc[csSleepEnabled].as<bool>());
            else
                fxRegistry.enableSleep(false);      //this doesn't invoke effect changing because sleep state is initialized with false
        }
        //we need the sleep mode flag setup first to properly advance to next effect
        {
            CoreMutex lock(&fxRegistryMutex);
            const uint16_t sleepFxIndex = fxRegistry.findEffectIndex(FX_SLEEPLIGHT_ID);
            //set the desired effect directly, the fxSetup (caller of this method) will invoke transitionEffect after more setup is done
            fxRegistry.desiredEffectIndex = fxRegistry.isAsleep() ? sleepFxIndex : fx == sleepFxIndex ? random16(fxRegistry.effectsCount) : fx;
        }
        if (doc[csBroadcast].is<bool>())
            fxBroadcastEnabled = doc[csBroadcast].as<bool>();

        bool sleepEnabled = false;
        {
            CoreMutex lock(&fxRegistryMutex);
            sleepEnabled = fxRegistry.isSleepEnabled();
        }
        log_info(F("System state restored from %s [%zu bytes]: autoFx=%s, randomSeed=%d, nextEffect=%hu, brightness=%hu (auto adjust), holiday=%s (auto=%s), sleepEnabled=%s, broadcast=%s"),
            stateFileName, stateSize, StringUtils::asString(autoAdvance), seed, fx, stripBrightness, holidayToString(paletteFactory.getHoliday()),
            StringUtils::asString(paletteFactory.isAuto()), StringUtils::asString(sleepEnabled), StringUtils::asString(fxBroadcastEnabled));
        doc.clear();
    }
    delete json;
}

void saveFxState() {
    JsonDocument doc;
    doc[csRandomSeed] = random16_get_seed();
    {
        CoreMutex lock(&fxRegistryMutex);
        doc[csAutoFxRoll] = fxRegistry.isAutoRoll();
        doc[csCurFx] = fxRegistry.curEffectPos();
    }
    doc[csStripBrightness] = stripBrightness.load();
    doc[csColorTheme] = holidayToString(paletteFactory.getHoliday());
    doc[csAutoColorAdjust] = paletteFactory.isAuto();
    {
        CoreMutex lock(&fxRegistryMutex);
        doc[csSleepEnabled] = fxRegistry.isSleepEnabled();
    }
    doc[csBroadcast] = fxBroadcastEnabled.load();
    const auto str = new String();
    str->reserve(measureJson(doc));
    serializeJson(doc, *str);
    if (!SyncFsImpl.writeFile(stateFileName, str))
        log_error(F("Failed to create/write the status file %s"), stateFileName);
    doc.clear();
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
    //flushing to strip may cause a short blink if called mid-effect
    const bool flushStrip = sysInfo->isSysStatus(SysStatus::Ntp) && !fxRegistry.isAsleep();
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
    //Strip brightness adjustment needs the time, that's why it is done in fxRun periodically. In the beginning we'll use the value from the saved state
    readFxState();
    // transEffect.setup(); -- done in EffectRegistry::transitionEffect

    shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
    //ensure the current effect is instantiated and moved to the setup state
    {
        CoreMutex lock(&fxRegistryMutex);
        fxRegistry.transitionEffect();
    }
    
    // With the new design, we need to manually trigger the first effect creation since loop hasn't run yet
    // The first call to loop() will detect activeEffect is nullptr and create it
    // For now during setup, we need to ensure activeEffect is created
    {
        CoreMutex lock(&fxRegistryMutex);
        fxRegistry.loop();
    }

    //generate and cache the FX config data
    JsonDocument doc;
    const auto hldList = doc["holidayList"].to<JsonArray>();
    for (uint8_t hi = None; hi <= NewYear; hi++)
        hldList.add(holidayToString(static_cast<Holiday>(hi)));
    const auto fxArray = doc["fx"].to<JsonArray>();
    {
        CoreMutex lock(&fxRegistryMutex);
        fxRegistry.describeConfig(fxArray);
    }
    auto *str = new String();
    str->reserve(measureJson(doc));
    serializeJson(doc, *str);
    if (!SyncFsImpl.writeFile(fxCfgFileName, str))
        log_error(F("Cannot save FxConfig JSON file %s"), fxCfgFileName);
    {
        CoreMutex lock(&fxRegistryMutex);
        if (const LedEffect *curFx = fxRegistry.getCurrentEffect(); curFx != nullptr)
            log_info(F("Fx Setup done - current effect %s (%d) set desired state to Setup (%d)"), curFx->name(), curFx->getRegistryIndex(), Setup);
        else
            log_warn(F("Fx Setup done - current effect is null after setup"));
    }
    delete str;
    doc.clear();
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
    if (tpl.size() > 17) {
        tpl(9, 11) = UPGRADE_COLOR_2;
        tpl(12, 16) = UPGRADE_COLOR_4;
        tpl(17, tpl.size() - 1) = UPGRADE_COLOR_5;
    }
    replicateSet(tpl, others);
    FastLED.show(stripBrightness);
}

void updateBrightness() {
    const uint8_t oldBrightness = stripBrightness;
    stripBrightness = adjustStripBrightness();
    if (oldBrightness != stripBrightness) {
        log_info(F("Strip brightness updated from %d to %d"), oldBrightness, stripBrightness);
    }
}

void switchToRandomEffect() {
    log_info(F("Attempting switching effect to a new random one"));
    {
        CoreMutex lock(&fxRegistryMutex);
        fxRegistry.nextRandomEffectPos();
    }
    shuffleIndexes(stripShuffleIndex, NUM_PIXELS);
    saveFxState();
}

//FX Run -------
void fx_run() {
    static bool isFirmwareUpgrading = false;
    watchdog_hw->scratch[kFxStageScratchIndex] = kFxStageEnter;

    FxActionMessage msg{};
    if (pdTRUE == xQueueReceive(fxQueue, &msg, 0)) {
        switch (msg.action) {
            case AUTO_FX: {
                CoreMutex lock(&fxRegistryMutex);
                fxRegistry.autoRoll(static_cast<bool>(msg.data));
                break;
            }
            case MANUAL_FX: {
                CoreMutex lock(&fxRegistryMutex);
                fxRegistry.nextEffectPos(static_cast<uint16_t>(msg.data));
                break;
            }
            case COLOR_THEME: paletteFactory.setHoliday(static_cast<Holiday>(msg.data)); break;
            case SLEEP_ENABLED: {
                CoreMutex lock(&fxRegistryMutex);
                fxRegistry.enableSleep(static_cast<bool>(msg.data));
                break;
            }
            case SLEEP_STATE: {
                CoreMutex lock(&fxRegistryMutex);
                fxRegistry.setSleepState(static_cast<bool>(msg.data));
                break;
            }
            case SAVE_STATE: saveFxState(); break;
            case STRIP_BRIGHTNESS: {
                const auto br = static_cast<uint8_t>(msg.data);
                stripBrightnessLocked = br > 0;
                stripBrightness = stripBrightnessLocked ? br : adjustStripBrightness();
                break;
            }
            default:
                log_error(F("Fx Action %hu not supported"), msg.action);
        }
    }
    watchdog_hw->scratch[kFxStageScratchIndex] = kFxStageAfterQueue;

    if (ulTaskNotifyTake(pdTRUE, 0) == OTA_UPGRADE_NOTIFY) {
        log_info(F("OTA upgrade light pattern"));
        isFirmwareUpgrading = true;
    }
    watchdog_hw->scratch[kFxStageScratchIndex] = kFxStageAfterOtaCheck;

    if (isFirmwareUpgrading) {
        watchdog_hw->scratch[kFxStageScratchIndex] = kFxStageFirmwareUpgrade;
        displayFirmwareUpgradePattern();
        HealthMonitor::checkIn(HEALTH_FX);
        watchdog_hw->scratch[kFxStageScratchIndex] = kFxStageAfterPing;
        return;
    }

    EVERY_N_SECONDS(BRIGHTNESS_CHECK_INTERVAL_SECONDS) {
        updateBrightness();
    }

    EVERY_N_MINUTES(EFFECT_SWITCH_INTERVAL_MINUTES) {
        switchToRandomEffect();
    }

    watchdog_hw->scratch[kFxStageScratchIndex] = kFxStageBeforeLoop;
    {
        CoreMutex lock(&fxRegistryMutex);
        fxRegistry.loop();
    }
    watchdog_hw->scratch[kFxStageScratchIndex] = kFxStageAfterLoop;
    HealthMonitor::checkIn(HEALTH_FX);
    watchdog_hw->scratch[kFxStageScratchIndex] = kFxStageAfterPing;
}

// FxSchedule functions
void wakeup() {
    const FxActionMessage msg = {SLEEP_STATE, 0};
    if (const BaseType_t qResult = xQueueSend(fxQueue, &msg, 0); qResult != pdTRUE)
        log_error(F("Error sending SLEEP_STATE(false) message to FX queue - error %ld"), qResult);
}

void bedtime() {
    bool sleepEnabled = false;
    {
        CoreMutex lock(&fxRegistryMutex);
        sleepEnabled = fxRegistry.isSleepEnabled();
    }
    if (!sleepEnabled) {
        log_warn(F("Bedtime alarm triggered, sleep mode is disabled - no changes"));
        return;
    }
    const FxActionMessage msg = {SLEEP_STATE, 1};
    if (const BaseType_t qResult = xQueueSend(fxQueue, &msg, 0); qResult != pdTRUE)
        log_error(F("Error sending SLEEP_STATE(true) message to FX queue - error %ld"), qResult);
}

