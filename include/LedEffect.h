// Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef LEDEFFECT_H
#define LEDEFFECT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include "global.h"

// Time Performance by Mark Kriegsman of FastLED at https://gist.github.com/kriegsman/a916be18d32ec675fea8

#define TC(HOURS,MINUTES,SECONDS) \
((uint32_t)(((uint32_t)((HOURS)*(uint32_t)(3600000))) + \
((uint32_t)((MINUTES)*(uint32_t)(60000))) + \
((uint32_t)((SECONDS)*(uint32_t)(1000)))))

#define AT(HOURS,MINUTES,SECONDS) if( atTC(TC(HOURS,MINUTES,SECONDS)) )
#define FROM(HOURS,MINUTES,SECONDS) if( fromTC(TC(HOURS,MINUTES,SECONDS)) )

enum OpMode:uint8_t { TurnOff, Chase };
enum EffectState:uint8_t { Idle, Setup, Running, WindDown, Cleanup };

class LedEffect;
/**
 * Represents the identifier of the effect along with a description. The identifier is generally short, around 4 characters,
 * and serves as a concise reference to the specific effect.
 */
struct EffectDescription {
    const char* id;             //short (4 characters -ish) identifier/abbreviation of the effect
    const char* description;    //description of the effect (keep it brief)
};
// Effect factory function type - creates a new effect instance
using EffectFactory = std::function<LedEffect*()>;

// Effect metadata structure
struct EffectInfo {
    EffectFactory factory;
    EffectDescription desc{};
    uint8_t selectionWeight{};
};


//base class for all effects
class LedEffect {
public:
    explicit LedEffect(const EffectInfo& identity);
    virtual ~LedEffect() = default;

    // Public interface
    [[nodiscard]] uint16_t getRegistryIndex() const;
    void setRegistryIndex(uint16_t index);
    [[nodiscard]] const char* name() const;
    [[nodiscard]] const char* description() const;
    [[nodiscard]] bool isInTransitionState() const;
    [[nodiscard]] bool isIdle() const;
    [[nodiscard]] bool isRunning() const;
    void loop();
    void desiredState(EffectState dst);
    bool atTC(uint32_t time);
    bool fromTC(uint32_t time);
    void restartPerformance();

    // Virtual methods
    virtual void baseConfig(JsonObject& json) const;
    virtual void setup();
    virtual void run() = 0;  // Pure virtual function
    virtual bool windDown();
    virtual void cleanup() { }  // Override in effects that need resource cleanup
    [[nodiscard]] EffectState getState() const { return state; }
    /**
     * The weight this effect has when random selection is engaged. Subclasses can customize this value by
     * e.g., the current holiday, time, etc., hence changing/reshaping the chances of selecting an effect
     * @return a value between 1 and 255. If returning 0, this effectively removes the effect from random selection.
     */
    [[nodiscard]] virtual uint8_t selectionWeight() const { return 1; }

protected:
    uint32_t lastTimeCodeDoneAt = 0;
    uint32_t lastTimeCodeDoneFrom = 0;
    uint32_t timeCode = 0;
    uint32_t timeCodeBase = 0;

    void updateTimeCode();

private:
    // Member variables
    EffectState state;
    const EffectDescription& identity;
    uint16_t registryIndex = 0;

    // Private methods
    [[nodiscard]] static EffectState getNextState(EffectState current, EffectState desired);
    void nextState();

    // Handler methods
    void handleSetup();
    void handleRunning();
    void handleWindDown();
    void handleCleanup();
    void handleIdle();
};

#endif //LEDEFFECT_H
