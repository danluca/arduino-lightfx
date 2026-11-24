//
// Copyright (c) by Dan Luca. All rights reserved
//
#pragma once
#include <cstdint>

class MeshFxSession {
    uint16_t previousEffect_ = 0;
    bool previousAutoRoll_ = false;
    bool active_ = false;

public:
    [[nodiscard]] bool active() const { return active_; }
    [[nodiscard]] uint16_t previousEffect() const { return previousEffect_; }
    [[nodiscard]] bool previousAutoRoll() const { return previousAutoRoll_; }

    bool begin(const uint16_t effect, const bool autoRoll) {
        if (active_)
            return false;
        previousEffect_ = effect;
        previousAutoRoll_ = autoRoll;
        active_ = true;
        return true;
    }

    void clear() { active_ = false; }
};

[[nodiscard]] constexpr bool meshControlTimedOut(const uint32_t nowMs, const uint32_t lastActiveMs, const uint32_t timeoutMs) {
    return nowMs - lastActiveMs >= timeoutMs;
}

class MeshFxActivity {
    uint32_t lastActivityMs_ = 0;
    bool active_ = false;

public:
    [[nodiscard]] bool active() const { return active_; }

    void record(const uint32_t nowMs) {
        lastActivityMs_ = nowMs;
        active_ = true;
    }

    void clear() { active_ = false; }

    [[nodiscard]] bool timedOut(const uint32_t nowMs, const uint32_t timeoutMs) const {
        return active_ && meshControlTimedOut(nowMs, lastActivityMs_, timeoutMs);
    }
};

// FX-task-only helpers shared by both boards. Registry restoration preserves sleep scheduling.
template<typename Registry>
void applyMeshFxState(MeshFxSession &session, Registry &registry, uint16_t effect) {
    (void)session.begin(registry.effectPosToRestore(), registry.isAutoRoll());
    registry.autoRoll(false);
    if (registry.effectPosToRestore() != effect)
        registry.restoreEffectPos(effect);
}

template<typename Registry>
void restoreMeshFxState(MeshFxSession &session, Registry &registry) {
    if (!session.active()) return;
    const uint16_t effect = session.previousEffect();
    const bool automatic = session.previousAutoRoll();
    session.clear();
    registry.autoRoll(automatic);
    registry.restoreEffectPos(effect);
}
