// Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
//
#include "efx_setup.h"      //cheating on the includes - we need both LedEffect and efx_setup, but efx_setup includes LedEffect
#include <variant>
#include <map>
#include <vector>
#include <memory>
#include "sysinfo.h"
#include "timeutil.h"
#include "transition.h"

struct StateTransition {
    EffectState current;
    EffectState desired;
    EffectState next;
};

// Simplified transition table
static const std::vector<StateTransition> transitions = {
    {Setup, Running, Running},
    {Setup, Idle, Cleanup},
    {Setup, Cleanup, Cleanup},
    {Running, WindDown, WindDown},
    {Running, Cleanup, WindDown},
    {Running, Idle, WindDown},
    {Running, Setup, WindDown},
    {WindDown, Cleanup, Cleanup},
    {WindDown, Idle, Cleanup},
    {WindDown, Setup, Cleanup},
    {WindDown, Running, Running},
    {Cleanup, Idle, Idle},
    {Cleanup, Setup, Setup},
    {Cleanup, Running, Setup},
    {Idle, Running, Setup},
    {Idle, Setup, Setup}
};
// Simplified state progression map
static const std::map<EffectState, EffectState> nextStateMap = {
    {Idle, Setup},
    {Setup, Running},
    {Running, WindDown},
    {WindDown, Cleanup},
    {Cleanup, Idle}
};

LedEffect::LedEffect(const EffectInfo& identity) : state(Idle), identity(identity.desc) {
    registryIndex = 0; // Will be set when factory creates the instance
}

uint16_t LedEffect::getRegistryIndex() const {
    return registryIndex;
}

void LedEffect::setRegistryIndex(const uint16_t index) {
    registryIndex = index;
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
    return identity.id;
}

const char* LedEffect::description() const {
    return identity.description;
}

bool LedEffect::isInTransitionState() const {
    return state == WindDown;
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

bool LedEffect::windDown() {
    return transEffect.transition();
}

void LedEffect::loop() {
    switch (state) {
        case Idle: handleIdle(); break;
        case Setup: handleSetup(); break;
        case Running: handleRunning(); break;
        case WindDown: handleWindDown(); break;
        case Cleanup: handleCleanup(); break;
    }
}

/**
 * Returns the next state on the path from current to desired
 * @param current current state
 * @param desired desired state
 * @return the next state on the path from current to desired
 */
EffectState LedEffect::getNextState(const EffectState current, const EffectState desired) {
    if (current == desired) return current;
    for (const auto&[trCurrent, trDesired, trNext] : transitions) {
        if (trCurrent == current && trDesired == desired) {
            return trNext;
        }
    }
    if (const auto it = nextStateMap.find(current); it != nextStateMap.end())
        return it->second;
    return current;
}

/**
 * Advances the current state of the LED effect to the next state in the path to the desired state provided.
 * If already at the desired state, simply returns
 * @param dst final desired state
 */
void LedEffect::desiredState(const EffectState dst) {
    if (const EffectState nextState = getNextState(state, dst); nextState != state) {
        state = nextState;
    }
}

bool LedEffect::atTC(const uint32_t time) {
    bool shouldExecute = false;
    if (timeCode >= time) {
        if (lastTimeCodeDoneAt < time) {
            shouldExecute = true;
            lastTimeCodeDoneAt = time;
        }
    }
    return shouldExecute;
}

bool LedEffect::fromTC(const uint32_t time) {
    bool shouldUpdate = false;
    if (timeCode >= time) {
        if (lastTimeCodeDoneFrom <= time) {
            shouldUpdate = true;
            lastTimeCodeDoneFrom = time;
        }
    }
    return shouldUpdate;
}

void LedEffect::updateTimeCode() {
    timeCode = millis() - timeCodeBase;
}

void LedEffect::restartPerformance() {
    lastTimeCodeDoneAt = 0;
    lastTimeCodeDoneFrom = 0;
    timeCode = 0;
    timeCodeBase = millis();
}

/**
 * Advances the current state of the LED effect to the next state based on the predefined state transition map.
 */
void LedEffect::nextState() {
    if (const auto it = nextStateMap.find(state); it != nextStateMap.end()) {
        state = it->second;
    }
}

// Handler implementations
void LedEffect::handleSetup() {
    log_info(F("Starting setup for effect: %s [%d]"), name(), getRegistryIndex());
    logHeapStats();

    setup();
    restartPerformance();

    logHeapStats();
    log_info(F("Effect %s [%d] completed setup, moving to running state"), name(), getRegistryIndex());
    nextState();
}

void LedEffect::handleRunning() {
    updateTimeCode();
    run();
}

void LedEffect::handleWindDown() {
    if (windDown()) {
        log_info(F("Effect %s [%d] completed WindDown"), name(), getRegistryIndex());
        nextState();
    }
}

void LedEffect::handleCleanup() {
    log_info(F("Starting cleanup for effect: %s [%d]"), name(), getRegistryIndex());
    logHeapStats();

    cleanup();

    logHeapStats();
    log_info(F("Effect %s [%d] completed cleanup"), name(), getRegistryIndex());
    nextState();
}

void LedEffect::handleIdle() {
    // No-op
}
