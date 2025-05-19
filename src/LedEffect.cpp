// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#include "efx_setup.h"
#include "PaletteFactory.h"
#include "timeutil.h"
#include "transition.h"

uint16_t LedEffect::getRegistryIndex() const {
    return registryIndex;
}

void LedEffect::baseConfig(JsonObject &json) const {
    json["description"] = description();
    json["name"] = name();
    json["registryIndex"] = getRegistryIndex();
    json["palette"] = holidayToString(paletteFactory.getHoliday());
}

LedEffect::LedEffect(const char *description) : state(Idle), desc(description) {
    //copy into id field the prefix of description (e.g. for a description 'FxA1: awesome lights', copies 'FxA1' into id)
    for (uint8_t i=0; i<LED_EFFECT_ID_SIZE; i++) {
        if (description[i] == ':' || description[i] == '\0') {
            id[i] = '\0';
            break;
        }
        id[i] = description[i];
    }
    //in case the prefix in description is larger, ensure id is null terminated (i.e. truncate)
    id[LED_EFFECT_ID_SIZE-1] = '\0';
    //register the effect
    registryIndex = fxRegistry.registerEffect(this);
}

const char *LedEffect::name() const {
    return id;
}

const char *LedEffect::description() const {
    return desc;
}

/**
 * Non-repeat by design. All the setup occurs in one blocking step.
 */
void LedEffect::setup() {
    resetGlobals();
}

/**
 * Performs the transition to off - by default a pause of 1 second. Subclasses can override the behavior - function is virtual
 * This is a repeat function - it is called multiple times while in TransitionBreak state, until it returns true.
 * @return true if transition has completed; false otherwise
 */
bool LedEffect::transitionBreak() {
    //if we need to customize the amount of pause between effects, make a global variable (same for all effects) or a local class member (custom for each effect)
    return millis() > (transOffStart + 1000);
}

/**
 * Called only once as the effect transitions into TransitionBreak state, before the loop calls to <code>transitionBreak</code>
 */
void LedEffect::transitionBreakPrep() {
    //nothing for now
}

/**
 * This is a repeat function - it is called multiple times while in WindDown state, until it returns true
 * @return true if transition has completed; false otherwise
 */
bool LedEffect::windDown() {
    return transEffect.transition();
}

/**
 * Called only once as the effect transitions into WindDown state, before the loop calls to <code>windDown</code>
 */
void LedEffect::windDownPrep() {
    CRGBSet strip(leds, NUM_PIXELS);
    strip.nblend(ColorFromPalette(targetPalette, random8(), 72, LINEARBLEND), 80);
    FastLED.show(stripBrightness);
    transEffect.prepare(random8());
}

/**
 * Re-entrant looping function
 */
void LedEffect::loop() {
    switch (state) {
        case Setup:
            setup();
            log_info(F("Effect %s [%d] completed setup, moving to running state"), name(), getRegistryIndex());
            nextState();
            break;    //one blocking step, non repeat
        case Running: run(); break;                 //repeat, called multiple times to achieve the light effects designed
        case WindDownPrep:
            windDownPrep();
            log_info(F("Effect %s [%d] completed WindDown Prep"), name(), getRegistryIndex());
            nextState();
            break;
        case WindDown:
            if (windDown()) {
                log_info(F("Effect %s [%d] completed WindDown"), name(), getRegistryIndex());
                nextState();
            }
            break;           //repeat, called multiple times to achieve the fade out for the current light effect
        case TransitionBreakPrep:
            transitionBreakPrep();
            log_info(F("Effect %s [%d] completed TransitionBreak Prep"), name(), getRegistryIndex());
            nextState();
            break;
        case TransitionBreak:
            if (transitionBreak()) {
                log_info(F("Effect %s [%d] completed TransitionBreak"), name(), getRegistryIndex());
                nextState();
            }
            break; //repeat, called multiple times to achieve the transition off for the current light effect
        case Idle: break;                           //no-op
    }
}

/**
 * Implementation of desired state - informs the state machine of the intended next state and causes it to react.
 * This means either transitioning to an interim state that precedes the desired state, or directly switch to desired state
 * @param dst intended next state
 */
void LedEffect::desiredState(EffectState dst) {
    if (state == dst)
        return;     //already there
    switch (state) {
        case Setup:
            switch (dst) {
                case Idle: state = dst; break;
                case Running:
                case WindDownPrep:
                case WindDown:
                case TransitionBreakPrep:
                case TransitionBreak: return;   //not a valid transition, Setup may not have completed
            }
            break;
        case Running:
            switch (dst) {
                case WindDownPrep:
                case WindDown:
                case TransitionBreakPrep:
                case TransitionBreak:
                case Idle:
                case Setup: state = WindDownPrep; break;
            }
            break;
        case WindDownPrep:
            switch (dst) {
                case WindDown:
                case TransitionBreak:
                case TransitionBreakPrep:
                case Setup:
                case Idle: state = WindDown; break;
                case Running: state = dst; break;
            }
            break;
        case WindDown:
            //any transitions here will cut short the in-progress windDown function
            switch (dst) {
                case TransitionBreakPrep:
                case TransitionBreak:
                case Idle:
                case Setup: state = TransitionBreakPrep; transOffStart = millis(); break;
                case Running: state = dst; break;
                case WindDownPrep: return;  //not a valid transition
            }
            break;
        case TransitionBreakPrep:
            switch (dst) {
                case Idle:
                case Setup: state = Idle; break;
                case Running: state = Setup; break;
                case TransitionBreak: state = dst; break;
                case WindDown:
                case WindDownPrep: return;  //not a valid transition
            }
            break;
        case TransitionBreak:
            //any transitions here will cut short the in-progress transitionOff function
            switch (dst) {
                case Idle:
                case Setup: state = Idle; break;
                case Running: state = Setup; break;
                case TransitionBreakPrep:
                case WindDownPrep:
                case WindDown: return;  //not a valid transition
            }
            break;
        case Idle:
            switch (dst) {
                case Running:
                case Setup: state = Setup; break;
                case WindDownPrep:
                case WindDown:
                case TransitionBreakPrep:
                case TransitionBreak: return;   //not a valid transition
            }
            break;
    }
}

void LedEffect::nextState() {
    switch (state) {
        case Setup: state = Running; break;
        case Running: state = WindDownPrep; break;
        case WindDownPrep: state = WindDown; break;
        case WindDown: state = TransitionBreakPrep; transOffStart = millis(); break;
        case TransitionBreakPrep: state = TransitionBreak; break;
        case TransitionBreak: state = Idle; break;
        case Idle: state = Setup; break;
    }
}

