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

// There are two kinds of things you can put into this performance:
// "FROM" and "AT".
//
// * "FROM" means starting FROM this time AND CALLING IT REPEATEDLY until the next "FROM" time comes.
//
// * "AT" means do this ONE TIME ONLY "AT" the designated time.
//
// At least one of the FROM clauses will ALWAYS be executed.
// In the transitional times, TWO pieces of code will be executed back to back.
// For example, if one piece says "FROM(0,0,1.000) {DrawRed()}" and another says
// "FROM(0,0,2.000) {FlashBlue();}", what you'll get is this:
//   00:00:01.950  -> calls DrawRed
//   00:00:01.975  -> calls DrawRed
//   00:00:02.000  -> calls DrawRed AND calls FlashBlue !
//   00:00:02.025  -> calls FlashBlue
//   00:00:02.050  -> calls FlashBlue
// In most cases, this probably isn't significant in practice, but it's important to note.  It could be avoided by
// listing the sequence steps in reverse chronological order, but that makes it hard to read.
#define AT(HOURS,MINUTES,SECONDS) if( atTC(TC(HOURS,MINUTES,SECONDS)) )
#define FROM(HOURS,MINUTES,SECONDS) if( fromTC(TC(HOURS,MINUTES,SECONDS)) )

enum OpMode:uint8_t { TurnOff, Chase };
enum EffectState:uint8_t {
    Idle,       //stable state
    Setup,      //transient state
    Running,    //stable state
    WindDown,   //transient state
    Cleanup     //transient state
};

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
    virtual ~LedEffect();

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
    bool atTC(uint32_t time);       //use with AT macro
    bool fromTC(uint32_t time);     //use with FROM macro
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
    EffectState lastProcessedState = Idle;  // Track last state to detect state entries

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
