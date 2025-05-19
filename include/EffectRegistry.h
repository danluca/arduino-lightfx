// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef EFFECTREGISTRY_H
#define EFFECTREGISTRY_H

#include <Arduino.h>
#include "LedEffect.h"
#include "fixed_queue.h"
#include "global.h"

class EffectRegistry {
    std::deque<LedEffect*> effects{};
    FixedQueue<uint16_t, MAX_EFFECTS_HISTORY> lastEffects{};
    uint16_t currentEffect = 0;
    uint16_t effectsCount = 0;
    uint16_t lastEffectRun = 0;
    uint16_t sleepEffect = 0;
    bool autoSwitch = true;
    bool sleepState = false;
    bool sleepModeEnabled = false;

public:
    EffectRegistry() = default;

    [[nodiscard]] LedEffect *getCurrentEffect() const;

    [[nodiscard]] LedEffect *getEffect(uint16_t index) const;

    uint16_t nextEffectPos(uint16_t efx);

    uint16_t nextEffectPos(const char* id);

    uint16_t nextEffectPos();

    [[nodiscard]] uint16_t curEffectPos() const;

    uint16_t nextRandomEffectPos();

    void transitionEffect() const;

    uint16_t registerEffect(LedEffect *effect);

    LedEffect* findEffect(const char* id) const;

    [[nodiscard]] uint16_t size() const;

    void setup() const;

    void loop();

    void describeConfig(const JsonArray &json) const;

    void pastEffectsRun(const JsonArray &json);

    void autoRoll(bool switchType = true);

    [[nodiscard]] bool isAutoRoll() const;

    [[nodiscard]] bool isSleepEnabled() const;

    void enableSleep(bool bSleep);

    [[nodiscard]] bool isAsleep() const;

    void setSleepState(bool sleepFlag);
    friend void readFxState();
    friend void saveFxState();
};

extern EffectRegistry fxRegistry;

#endif //EFFECTREGISTRY_H
