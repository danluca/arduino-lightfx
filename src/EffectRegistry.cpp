// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#include "efx_setup.h"
#include "FxSchedule.h"
#include "transition.h"
#include "comms.h"


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
    const uint16_t fxIndex = effectsCount - 1;
    if (strcmp(FX_SLEEPLIGHT_ID, effect->name()) == 0)
        sleepEffect = fxIndex;
    log_info(F("Effect [%s] registered successfully at index %hu"), effect->name(), fxIndex);
    return fxIndex;
}

LedEffect *EffectRegistry::findEffect(const char *id) const {
    for (auto &fx : effects) {
        if (strcmp(id, fx->name()) == 0)
            return fx;
    }
    return nullptr;
}

void EffectRegistry::setSleepState(const bool sleepFlag) {
    if (sleepState != sleepFlag) {
        sleepState = sleepFlag;
        log_info(F("Switching to sleep state %s (sleep mode enabled %s)"), StringUtils::asString(sleepState), StringUtils::asString(sleepModeEnabled));
        if (sleepState) {
            nextEffectPos(FX_SLEEPLIGHT_ID);
        } else {
            if (lastEffects.size() < 2)
                nextRandomEffectPos();
            else {
                const uint16_t prevFx = *(lastEffects.end()-2); //the second entry in the queue (from the inserting point - the end) is the previous effect before the sleep mode
                currentEffect = prevFx;
                transitionEffect();
            }
        }
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
 * Sets the desired state for all effects to setup. It will essentially make each effect ready to run if invoked - the state machine will ensure the setup() is called first
 */
void EffectRegistry::setup() const {
    for (auto &fx : effects)
        fx->desiredState(Setup);
    transEffect.setup();
}

void EffectRegistry::loop() {
    //if effect has changed, re-run the effect's setup
    if ((lastEffectRun != currentEffect) && (effects[lastEffectRun]->getState() == Idle)) {
        log_info(F("Effect change: from index %d [%s] to %d [%s]"),
                lastEffectRun, effects[lastEffectRun]->description(), currentEffect, effects[currentEffect]->description());
        lastEffectRun = currentEffect;
        lastEffects.push(lastEffectRun);
        postFxChangeEvent(lastEffectRun);
    }
    effects[lastEffectRun]->loop();
}

void EffectRegistry::describeConfig(const JsonArray &json) const {
    for (auto & effect : effects) {
        auto fxJson = json.add<JsonObject>();
        effect->baseConfig(fxJson);
    }
}

LedEffect *EffectRegistry::getEffect(const uint16_t index) const {
    return effects[capu(index, effectsCount-1)];
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
    for (const auto &fxIndex: lastEffects)
        (void)json.add(getEffect(fxIndex)->name());
}
