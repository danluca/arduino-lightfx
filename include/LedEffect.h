// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef LEDEFFECT_H
#define LEDEFFECT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "global.h"

enum OpMode:uint8_t { TurnOff, Chase };
enum EffectState:uint8_t {Setup, Running, WindDownPrep, WindDown, TransitionBreakPrep, TransitionBreak, Idle};

//base class for all effects
class LedEffect {
public:
    explicit LedEffect(const char* description);
    virtual ~LedEffect() = default;

    // Public interface
    [[nodiscard]] uint16_t getRegistryIndex() const;
    [[nodiscard]] const char* name() const;
    [[nodiscard]] const char* description() const;
    [[nodiscard]] bool isInTransitionState() const;
    [[nodiscard]] bool isIdle() const;
    [[nodiscard]] bool isRunning() const;
    void loop();
    void desiredState(EffectState dst);

    // Virtual methods
    virtual void baseConfig(JsonObject& json) const;
    virtual void setup();
    virtual void run() = 0;  // Pure virtual function
    virtual bool transitionBreak();
    virtual void transitionBreakPrep();
    virtual bool windDown();
    virtual void windDownPrep();
    [[nodiscard]] EffectState getState() const { return state; }
    /**
     * The weight this effect has when random selection is engaged. Subclasses can customize this value by
     * e.g., the current holiday, time, etc., hence changing/reshaping the chances of selecting an effect
     * @return a value between 1 and 255. If returning 0, this effectively removes the effect from random selection.
     */
    [[nodiscard]] virtual uint8_t selectionWeight() const { return 1; }

private:
    // Member variables
    EffectState state;
    const char* desc;
    char id[LED_EFFECT_ID_SIZE] {};
    uint16_t registryIndex = 0;
    uint32_t transOffStart = 0;

    // Private methods
    [[nodiscard]] static EffectState getNextState(EffectState current, EffectState desired);
    void extractId(const char* description);
    void nextState();

    // Handler methods
    void handleSetup();
    void handleRunning();
    void handleWindDownPrep();
    void handleWindDown();
    void handleTransitionBreakPrep();
    void handleTransitionBreak();
    void handleIdle();
};

#endif //LEDEFFECT_H
