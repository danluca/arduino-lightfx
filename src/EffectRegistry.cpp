// Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
//
#include "efx_setup.h"
#include "FxSchedule.h"
#include "transition.h"
#include "comms.h"


// EffectRegistry
EffectRegistry::~EffectRegistry() {
    delete currentEffectInstance;
    delete lastEffectInstance;
}

LedEffect *EffectRegistry::getCurrentEffect() const {
    return currentEffectInstance;
}

const EffectInfo* EffectRegistry::getEffectInfo(const uint16_t index) const {
    return effectInfos[index % effectsCount];
}

uint16_t EffectRegistry::nextEffectPos(const char *id) {
    for (size_t x = 0; x < effectInfos.size(); x++) {
        if (strcmp(id, effectInfos[x]->desc.id) == 0) {
            currentEffect = x;
            transitionEffect();
            return lastEffectRun;
        }
    }
    return 0;
}

uint16_t EffectRegistry::nextEffectPos(const uint16_t efx) {
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
        for (const auto &info : effectInfos)
            totalSelectionWeight += info->selectionWeight;
        uint16_t rnd = random16(0, totalSelectionWeight);
        for (uint16_t i = 0; i < effectsCount; ++i) {
            rnd = qsuba(rnd, effectInfos[i]->selectionWeight);
            if (rnd == 0) {
                currentEffect = i;  //sleep effect weight is 0, so it cannot be chosen randomly
                break;
            }
        }
        transitionEffect();
    }
    return currentEffect;
}

/**
 * Manages the transition between LED effects in the EffectRegistry.
 *
 * This method gracefully transitions from the currently running effect to a new effect,
 * ensuring a smooth wind-down of the old effect and proper initialization of the new one.
 * It handles the necessary cleanup of the old effect instance, prepares any transition visuals,
 * and creates or updates the new effect instance as required.
 *
 * If the current effect is different from the last effect run, the previously running effect
 * is set to the Idle state, and a transition effect is initialized with a random color
 * blending into the target palette. The new effect is instantiated and transitioned to
 * the Running state to begin execution.
 *
 * This function is invoked whenever the active effect is updated within the registry.
 */
void EffectRegistry::transitionEffect() {
    if (currentEffect != lastEffectRun) {
        // Wind down old effect
        if (lastEffectInstance)
            lastEffectInstance->desiredState(Idle);

        // Initialize transition effect for smooth wind-down
        ledSet.nblend(ColorFromPalette(targetPalette, random8(), 72, LINEARBLEND), 80);
        FastLED.show(stripBrightness);
        transEffect.prepare(random8());
    }
    // Create the new effect instance if needed
    if (!currentEffectInstance || currentEffect != lastEffectRun) {
        if (currentEffectInstance && currentEffectInstance != lastEffectInstance)
            delete currentEffectInstance;
        currentEffectInstance = effectInfos[currentEffect]->factory();
        currentEffectInstance->setRegistryIndex(currentEffect);
        currentEffectInstance->desiredState(Running);
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
        sleepEffect = fxIndex;
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
            beforeSleepEffect = nextEffectPos(FX_SLEEPLIGHT_ID);
        else
            nextEffectPos(beforeSleepEffect);
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
 * This method is responsible for managing the lifecycle of LED effects, ensuring that
 * effects are cleaned up and switched appropriately when the active effect changes.
 * It also invokes the execution logic for the currently running effect.
 *
 * The following operations are performed in this loop:
 * - If the current effect differs from the last effect that was running and the last
 *   effect has transitioned to the Idle state, the resources for the last effect instance
 *   are freed, and the effect is officially switched.
 * - A notification is posted to inform any external listeners about the effect change.
 * - The current effect is executed if it exists, and its state is checked to determine
 *   if it should be tracked for future cleanup.
 *
 * Key Events:
 * - Effect transitions occur when the active effect is updated.
 * - Memory is managed by cleaning up old effect instances that are no longer in use.
 * - The running state of effects is monitored to ensure smooth operation.
 */
void EffectRegistry::loop() {
    //if the effect has changed and the old effect is idle, clean it up and switch
    if ((lastEffectRun != currentEffect) && lastEffectInstance && (lastEffectInstance->getState() == Idle)) {
        log_info(F("Effect change: from index %d [%s] to %d [%s]"), lastEffectRun, effectInfos[lastEffectRun]->desc.id,
            currentEffect, effectInfos[currentEffect]->desc.id);
        // Delete old effect instance to free memory
        delete lastEffectInstance;
        lastEffectInstance = nullptr;
        lastEffectRun = currentEffect;
        lastEffects.push(lastEffectRun);
        postFxChangeEvent(lastEffectRun);
    }
    // Run the current effect if it exists
    if (currentEffectInstance) {
        currentEffectInstance->loop();
        // Track the running effect as the last effect for cleanup
        if (currentEffectInstance->getState() == Running && currentEffectInstance != lastEffectInstance) {
            lastEffectInstance = currentEffectInstance;
        }
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

void EffectRegistry::pastEffectsRun(const JsonArray &json) {
    for (const auto &fxIndex: lastEffects) {
        if (fxIndex < effectsCount)
            (void)json.add(effectInfos[fxIndex]->desc.id);
    }
}
