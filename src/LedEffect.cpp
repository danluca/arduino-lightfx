// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#include "efx_setup.h"      //cheating on the includes - we need both LedEffect and efx_setup, but efx_setup includes LedEffect
#include <variant>
#include <map>
#include <vector>
#include <memory>
#include "PaletteFactory.h"
#include "timeutil.h"
#include "transition.h"

struct StateTransition {
    EffectState current;
    EffectState desired;
    EffectState next;
};
// Define the transition table
static const std::vector<StateTransition> transitions = {
    {Setup, Running, Running},
    {Setup, Idle, Idle},
    {Running, WindDownPrep, WindDownPrep},
    {Running, WindDown, WindDownPrep},
    {Running, TransitionBreakPrep, WindDownPrep},
    {Running, TransitionBreak, WindDownPrep},
    {Running, Idle, WindDownPrep},
    {Running, Setup, WindDownPrep},
    {WindDownPrep, WindDown, WindDown},
    {WindDownPrep, TransitionBreak, WindDown},
    {WindDownPrep, TransitionBreakPrep, WindDown},
    {WindDownPrep, Setup, WindDown},
    {WindDownPrep, Idle, WindDown},
    {WindDownPrep, Running, Running},
    {WindDown, TransitionBreakPrep, TransitionBreakPrep},
    {WindDown, TransitionBreak, TransitionBreakPrep},
    {WindDown, Idle, TransitionBreakPrep},
    {WindDown, Setup, TransitionBreakPrep},
    {WindDown, Running, Running},
    {TransitionBreakPrep, Idle, Idle},
    {TransitionBreakPrep, Setup, Idle},
    {TransitionBreakPrep, Running, Setup},
    {TransitionBreakPrep, TransitionBreak, TransitionBreak},
    {TransitionBreak, Idle, Idle},
    {TransitionBreak, Setup, Idle},
    {TransitionBreak, Running, Setup},
    {Idle, Running, Setup},
    {Idle, Setup, Setup}
};
// Define the next state map
static const std::map<EffectState, EffectState> nextStateMap = {
    {Setup, Running},
    {Running, WindDownPrep},
    {WindDownPrep, WindDown},
    {WindDown, TransitionBreakPrep},
    {TransitionBreakPrep, TransitionBreak},
    {TransitionBreak, Idle},
    {Idle, Setup}
};

LedEffect::LedEffect(const char* description) : state(Idle), desc(description) {
    extractId(description);
    registryIndex = fxRegistry.registerEffect(this);
}

void LedEffect::extractId(const char* description) {
    size_t i;
    for (i = 0; i < LED_EFFECT_ID_SIZE - 1 && description[i] != ':' && description[i] != '\0'; ++i) {
        id[i] = description[i];
    }
    id[i] = '\0';
}

uint16_t LedEffect::getRegistryIndex() const {
    return registryIndex;
}

void LedEffect::baseConfig(JsonObject& json) const {
    const std::pair<const char*, std::variant<const char*, uint16_t>> configs[] = {
        {"description", description()},
        {"name", name()},
        {"registryIndex", getRegistryIndex()},
        // {"palette", holidayToString(paletteFactory.getHoliday())}
    };

    for (const auto& [key, value] : configs)
        std::visit([&json, key](auto&& arg) { json[key] = arg; }, value);

    //the basic would be below - above is some fancy code
    // json["description"] = description();
    // json["name"] = name();
    // json["registryIndex"] = getRegistryIndex();
    // json["palette"] = holidayToString(paletteFactory.getHoliday());
}

const char* LedEffect::name() const {
    return id;
}

const char* LedEffect::description() const {
    return desc;
}

bool LedEffect::isInTransitionState() const {
    return state == WindDown || state == TransitionBreak;
}

bool LedEffect::isIdle() const {
    return state == Idle;
}

bool LedEffect::isRunning() const {
    return state == Running;
}

void LedEffect::setup() {
    resetGlobals();
}

bool LedEffect::transitionBreak() {
    return millis() > (transOffStart + 1000);
}

void LedEffect::transitionBreakPrep() {
    // Default implementation - no operation
}

bool LedEffect::windDown() {
    return transEffect.transition();
}

void LedEffect::windDownPrep() {
    ledSet.nblend(ColorFromPalette(targetPalette, random8(), 72, LINEARBLEND), 80);
    FastLED.show(stripBrightness);
    transEffect.prepare(random8());
}

void LedEffect::loop() {
    switch (state) {
        case Setup: handleSetup(); break;
        case Running: handleRunning(); break;
        case WindDownPrep: handleWindDownPrep(); break;
        case WindDown: handleWindDown(); break;
        case TransitionBreakPrep: handleTransitionBreakPrep(); break;
        case TransitionBreak: handleTransitionBreak(); break;
        case Idle: handleIdle(); break;
    }
}

/**
 * Returns the next state on the path from current to desired
 * @param current current state
 * @param desired desired state
 * @return the next state on the path from current to desired
 */
EffectState LedEffect::getNextState(const EffectState current, const EffectState desired) {
    for (const auto&[trCurrent, trDesired, trNext] : transitions) {
        if (trCurrent == current && trDesired == desired) {
            return trNext;
        }
    }
    return current;
}

/**
 * Advances the current state of the LED effect to the next state in the path to the desired state provided.
 * If already at the desired state, simply returns
 * @param dst final desired state
 */
void LedEffect::desiredState(const EffectState dst) {
    if (state == dst) return;

    if (const EffectState nextState = getNextState(state, dst); nextState != state) {
        state = nextState;
        if (state == TransitionBreakPrep || state == TransitionBreak)
            transOffStart = millis();
    }
}

/**
 * Advances the current state of the LED effect to the next state based on the predefined state transition map.
 * If the updated state indicates that a transition break is about to start
 * (`TransitionBreakPrep` or `TransitionBreak`), it records the current time as the start of the transition break.
 */
void LedEffect::nextState() {
    if (const auto it = nextStateMap.find(state); it != nextStateMap.end()) {
        state = it->second;
        if (state == TransitionBreakPrep || state == TransitionBreak)
            transOffStart = millis();
    }
}

// Handler implementations
void LedEffect::handleSetup() {
    setup();
    log_info(F("Effect %s [%d] completed setup, moving to running state"), name(), getRegistryIndex());
    nextState();
}

void LedEffect::handleRunning() {
    run();
}

void LedEffect::handleWindDownPrep() {
    windDownPrep();
    log_info(F("Effect %s [%d] completed WindDown Prep"), name(), getRegistryIndex());
    nextState();
}

void LedEffect::handleWindDown() {
    if (windDown()) {
        log_info(F("Effect %s [%d] completed WindDown"), name(), getRegistryIndex());
        nextState();
    }
}

void LedEffect::handleTransitionBreakPrep() {
    transitionBreakPrep();
    log_info(F("Effect %s [%d] completed TransitionBreak Prep"), name(), getRegistryIndex());
    nextState();
}

void LedEffect::handleTransitionBreak() {
    if (transitionBreak()) {
        log_info(F("Effect %s [%d] completed TransitionBreak"), name(), getRegistryIndex());
        nextState();
    }
}

void LedEffect::handleIdle() {
    // No-op
}
