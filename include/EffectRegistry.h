// Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef EFFECTREGISTRY_H
#define EFFECTREGISTRY_H

#include <Arduino.h>
#include "LedEffect.h"
#include <fixed_queue.h>
#include "global.h"

class EffectRegistry {
    std::deque<const EffectInfo*> effectInfos{};
    FixedQueue<uint16_t, MAX_EFFECTS_HISTORY> lastEffects{};
    mutable mutex_t mutex{};
    LedEffect* activeEffect = nullptr;         // The effect currently being looped/managed
    uint16_t nextEffectIndex = 0;              // Index of the effect waiting to be created after activeEffect is done
    uint16_t desiredEffectIndex = 0;           // Currently requested effect index
    uint16_t lastEffectIndex = 0;
    uint16_t effectsCount = 0;
    uint16_t sleepEffectIndex = 0;
    uint16_t beforeSleepEffectIndex = 0;
    bool autoSwitch = true;
    bool sleepState = false;
    bool sleepModeEnabled = false;

public:
    EffectRegistry();
    ~EffectRegistry();

    [[nodiscard]] const EffectInfo* getEffectInfo(uint16_t index) const;

    uint16_t nextEffectPos(uint16_t efx);

    uint16_t nextEffectPos(const char* id);

    uint16_t nextEffectPos();

    [[nodiscard]] uint16_t curEffectPos() const;

    uint16_t nextRandomEffectPos();

    void transitionEffect();

    uint16_t registerEffect(const EffectInfo* info);

    uint16_t findEffectIndex(const char* id) const;

    [[nodiscard]] uint16_t size() const;

    void loop();

    void describeConfig(const JsonArray &json) const;

    void pastEffectsRun(const JsonArray &json);

    void autoRoll(bool switchType = true);

    [[nodiscard]] bool isAutoRoll() const;

    [[nodiscard]] bool isSleepEnabled() const;

    void enableSleep(bool bSleep);

    [[nodiscard]] bool isAsleep() const;

    void setSleepState(bool sleepFlag);
    void restoreDesiredEffectFromState(uint16_t fx);

private:
    uint16_t nextEffectPosUnlocked(uint16_t efx);
    uint16_t nextEffectPosUnlocked(const char *id);
    uint16_t nextEffectPosUnlocked();
    uint16_t nextRandomEffectPosUnlocked();
    void transitionEffectUnlocked();
    void setSleepStateUnlocked(bool sleepFlag);
};

extern EffectRegistry fxRegistry;

#endif //EFFECTREGISTRY_H
