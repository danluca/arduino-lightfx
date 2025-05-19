// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef LEDEFFECT_H
#define LEDEFFECT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "global.h"

enum OpMode { TurnOff, Chase };
enum EffectState:uint8_t {Setup, Running, WindDownPrep, WindDown, TransitionBreakPrep, TransitionBreak, Idle};
//base class/interface for all effects
class LedEffect {
protected:
    uint registryIndex = 0;
    EffectState state;
    ulong transOffStart = 0;
    const char* const desc;
    char id[LED_EFFECT_ID_SIZE] {};   //this is name of the class, max 5 characters (plus null terminal)
public:
    explicit LedEffect(const char* description);

    virtual void setup();

    virtual void run() = 0;

    virtual bool windDown();

    virtual void windDownPrep();

    virtual bool transitionBreak();

    virtual void transitionBreakPrep();

    virtual void desiredState(EffectState dst);

    virtual void nextState();

    virtual void loop();

    [[nodiscard]] const char *description() const;

    [[nodiscard]] const char *name() const;

    virtual void baseConfig(JsonObject &json) const;

    [[nodiscard]] uint16_t getRegistryIndex() const;

    [[nodiscard]] EffectState getState() const { return state; }

    /**
     * What weight does this effect have when random selection is engaged
     * Subclasses have the opportunity to customize this value by e.g. the current holiday, time, etc., hence changing/reshaping the chances of selecting an effect
     * @return a value between 1 and 255. If returning 0, this effectively removes the effect from random selection.
     */
    [[nodiscard]] virtual uint8_t selectionWeight() const {
        return 1;
    }

    virtual ~LedEffect() = default;     // Destructor
};

#endif //LEDEFFECT_H
