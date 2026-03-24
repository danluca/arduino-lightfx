// Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
//
#include "efx_setup.h"
#include "FxSchedule.h"
#include "transition.h"
#include "comms.h"


// EffectRegistry
EffectRegistry::~EffectRegistry() {
    delete activeEffect;
}

const EffectInfo* EffectRegistry::getEffectInfo(const uint16_t index) const {
    if (effectsCount == 0)
        return nullptr;
    return effectInfos[index % effectsCount];
}

String EffectRegistry::getEffectId(const uint16_t index) const {
    if (effectsCount == 0 || index >= effectsCount)
        return String("unknown");
    return String(effectInfos[index]->desc.id);
}

uint16_t EffectRegistry::nextEffectPos(const char *id) {
    for (size_t x = 0; x < effectInfos.size(); x++) {
        if (strcmp(id, effectInfos[x]->desc.id) == 0) {
            desiredEffectIndex = x;
            transitionEffect();
            return lastEffectIndex;
        }
    }
    return 0;
}

uint16_t EffectRegistry::nextEffectPos(const uint16_t efx) {
    desiredEffectIndex = capu(efx, effectsCount-1);
    transitionEffect();
    return lastEffectIndex;
}

uint16_t EffectRegistry::nextEffectPos() {
    if (!autoSwitch || sleepState)
        return desiredEffectIndex;
    desiredEffectIndex = inc(desiredEffectIndex, 1, effectsCount);
    //increment past the sleep effect, if landed on it
    if (desiredEffectIndex == sleepEffectIndex)
        desiredEffectIndex = inc(desiredEffectIndex, 1, effectsCount);
    transitionEffect();
    return lastEffectIndex;
}

uint16_t EffectRegistry::curEffectPos() const {
    return desiredEffectIndex;
}

uint16_t EffectRegistry::nextRandomEffectPos() {
    if (autoSwitch && !sleepState) {
        //weighted randomization of the next effect index
        uint16_t totalSelectionWeight = 0;
        for (const auto &info : effectInfos)
            totalSelectionWeight += info->selectionWeight;
        uint16_t rnd = random16(0, totalSelectionWeight);
        for (uint16_t i = 0; i < effectsCount; ++i) {
            rnd = qsuba(rnd, effectInfos[i]->selectionWeight);
            if (rnd == 0) {
                desiredEffectIndex = i;  //sleep effect weight is 0, so it cannot be chosen randomly
                break;
            }
        }
        log_info(F("Random effect selection: index %d [%s]"), desiredEffectIndex, effectInfos[desiredEffectIndex]->desc.id);
        transitionEffect();
    } else {
        log_info(F("Random effect selection skipped - auto switch %s, sleep state %s"), StringUtils::asString(autoSwitch), StringUtils::asString(sleepState));
    }
    return desiredEffectIndex;
}

/**
 * Manages the transition between LED effects in the EffectRegistry.
 *
 * This method initiates a transition from the currently running effect to a new effect.
 * The old effect is set to wind down gracefully, and a transition effect is displayed.
 * The new effect will be instantiated only after the old effect has completed its
 * wind-down and cleanup, minimizing heap fragmentation.
 *
 * This function is invoked whenever the active effect is updated within the registry.
 */
void EffectRegistry::transitionEffect() {
    if (desiredEffectIndex != lastEffectIndex) {
        // Store the index of the effect to create after the current one finishes
        nextEffectIndex = desiredEffectIndex;
        
        // Wind down the currently active effect
        if (activeEffect)
            activeEffect->desiredState(Idle);

        // Initialize transition effect for smooth wind-down
        ledSet.nblend(ColorFromPalette(targetPalette, random8(), 72, LINEARBLEND), 80);
        FastLED.show(stripBrightness);
        transEffect.prepare(random8());
        postFxChangeEvent(nextEffectIndex); //post the effect change to allow receiving boards to run their transition roughly in sync
    }
}

/**
 * Registers a new LED effect in the EffectRegistry.
 *
 * This method adds an LED effect, characterized by its description and factory implementation,
 * to the registry. Each effect is assigned a unique index upon registration. A weight can also
 * be provided to indicate the priority or probability of the effect being selected during random
 * selection.
 *
 * If the registered effect matches the predefined sleep light effect ID, it will be tagged
 * as the sleep effect within the registry.
 *
 * @param info The description of the effect, containing its ID, a brief description, a factory function that generates instances
 * of the effect, and a numeric value between 1 and 255 that determines the weight or priority of the effect for random selection.
 * @return The index of the registered effect in the EffectRegistry.
 */
uint16_t EffectRegistry::registerEffect(const EffectInfo* info) {
    effectInfos.push_back(info);
    effectsCount = effectInfos.size();
    const uint16_t fxIndex = effectsCount - 1;
    if (strcmp(FX_SLEEPLIGHT_ID, info->desc.id) == 0)
        sleepEffectIndex = fxIndex;
    log_info(F("Effect [%s] registered successfully at index %hu"), info->desc.id, fxIndex);
    return fxIndex;
}

uint16_t EffectRegistry::findEffectIndex(const char *id) const {
    for (size_t x = 0; x < effectInfos.size(); x++) {
        if (strcmp(id, effectInfos[x]->desc.id) == 0)
            return x;
    }
    return 0;
}

void EffectRegistry::setSleepState(const bool sleepFlag) {
    if (sleepState != sleepFlag) {
        sleepState = sleepFlag;
        log_info(F("Switching to sleep state %s (sleep mode enabled %s)"), StringUtils::asString(sleepState), StringUtils::asString(sleepModeEnabled));
        if (sleepState)
            beforeSleepEffectIndex = nextEffectPos(FX_SLEEPLIGHT_ID);
        else
            nextEffectPos(beforeSleepEffectIndex);
    } else
        log_info(F("Sleep state is already %s - no changes"), StringUtils::asString(sleepState));
}

void EffectRegistry::enableSleep(const bool bSleep) {
    sleepModeEnabled = bSleep;
    log_info(F("Sleep mode enabled is now %s"), StringUtils::asString(sleepModeEnabled));
    //determine the proper sleep status based on time
    setSleepState(sleepModeEnabled && !isAwakeTime(now()));
}

/**
 * Executes the primary event loop for the EffectRegistry.
 *
 * This method is responsible for managing the lifecycle of LED effects:
 * - Always calls loop on the active effect to allow it to run or transition
 * - Once the active effect reaches Idle state, deletes it and creates the next effect
 * - This deferred instantiation minimizes heap fragmentation by creating the new effect
 *   only after the old one is completely freed from memory
 *
 * Key Events:
 * - Effect transitions occur when the active effect winds down to Idle
 * - The new effect is created only after memory is freed from the old effect
 * - A notification is posted when the effect change is complete
 */
void EffectRegistry::loop() {
    // Always process the active effect (running or transitioning)
    if (activeEffect)
        activeEffect->loop();

    // Check if active effect has completed its transition to Idle
    if (activeEffect && activeEffect->getState() == Idle) {
        // Log the effect change
        if (lastEffectIndex != nextEffectIndex) {
            log_info(F("Effect change: from index %d [%s] to %d [%s]"), lastEffectIndex, effectInfos[lastEffectIndex]->desc.id,
                nextEffectIndex, effectInfos[nextEffectIndex]->desc.id);
        }
        
        // Delete the old effect to free memory
        delete activeEffect;
        activeEffect = nullptr;
        desiredEffectIndex = lastEffectIndex = nextEffectIndex;
        lastEffects.push(lastEffectIndex);
        // postFxChangeEvent(lastEffectIndex);
    }
    
    // Create the new effect if we don't have an active effect but have a pending one (startup or after transition)
    if (!activeEffect && nextEffectIndex < effectsCount) {
        activeEffect = effectInfos[nextEffectIndex]->factory();
        activeEffect->setRegistryIndex(nextEffectIndex);
        activeEffect->desiredState(Running);
    }
}

void EffectRegistry::describeConfig(const JsonArray &json) const {
    int index = 0;
    for (const auto & info : effectInfos) {
        auto fxJson = json.add<JsonObject>();
        fxJson["description"] = info->desc.description;
        fxJson["name"] = info->desc.id;
        fxJson["registryIndex"] = index++;
    }
}

void EffectRegistry::autoRoll(const bool switchType) {
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

void EffectRegistry::pastEffectsRun(const JsonArray &json) const {
    for (const auto &fxIndex: lastEffects) {
        if (fxIndex < effectsCount)
            (void)json.add(effectInfos[fxIndex]->desc.id);
    }
}

void EffectRegistry::restoreDesiredEffectFromState(const uint16_t fx) {
    uint16_t sleepFxIndex = 0;
    for (size_t x = 0; x < effectInfos.size(); x++) {
        if (strcmp(FX_SLEEPLIGHT_ID, effectInfos[x]->desc.id) == 0) {
            sleepFxIndex = x;
            break;
        }
    }
    if (effectsCount == 0)
        desiredEffectIndex = 0;
    else
        desiredEffectIndex = sleepState ? sleepFxIndex : fx == sleepFxIndex ? random16(effectsCount) : capu(fx, effectsCount - 1);
}
