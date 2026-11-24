#include <cassert>
#include <cstdint>
#include "mesh_fx_session.h"

struct Registry {
    uint16_t effect = 12;
    uint16_t beforeSleep = 12;
    bool automatic = true;
    bool asleep = false;
    unsigned transitions = 0;
    uint16_t effectPosToRestore() const { return asleep ? beforeSleep : effect; }
    bool isAutoRoll() const { return automatic; }
    void autoRoll(bool value) { automatic = value; }
    void restoreEffectPos(uint16_t value) {
        ++transitions;
        if (asleep) beforeSleep = value;
        else effect = value;
    }
};

int main() {
    Registry registry;
    MeshFxSession snapshot;
    applyMeshFxState(snapshot, registry, 24);
    assert(!registry.automatic && registry.effect == 24);
    applyMeshFxState(snapshot, registry, 24);
    assert(registry.transitions == 1); // Heartbeats cannot restart the animation.
    applyMeshFxState(snapshot, registry, 25);
    restoreMeshFxState(snapshot, registry);
    assert(registry.automatic && registry.effect == 12);
    registry.asleep = true;
    registry.effect = 0; // Sleep animation stays selected throughout the session.
    registry.beforeSleep = 18;
    applyMeshFxState(snapshot, registry, 24);
    assert(registry.effect == 0 && registry.beforeSleep == 24);
    restoreMeshFxState(snapshot, registry);
    assert(registry.effect == 0 && registry.beforeSleep == 18 && registry.automatic);

    MeshFxSession session;
    assert(!session.active());
    assert(session.begin(12, true));
    assert(session.active());
    assert(session.previousEffect() == 12);
    assert(session.previousAutoRoll());

    assert(!session.begin(24, false));
    assert(session.previousEffect() == 12);
    assert(session.previousAutoRoll());

    session.clear();
    assert(!session.active());
    assert(session.begin(24, false));
    assert(session.previousEffect() == 24);
    assert(!session.previousAutoRoll());
    session.clear(); // A manual override abandons the snapshot without restoration.
    assert(!session.active());

    constexpr uint32_t timeoutMs = 20u * 60u * 1000u;
    assert(!meshControlTimedOut(1000u + timeoutMs - 1u, 1000u, timeoutMs));
    assert(meshControlTimedOut(1000u + timeoutMs, 1000u, timeoutMs));
    assert(meshControlTimedOut(0x20u, 0xFFFFFFF0u, 48u));
    assert(!meshControlTimedOut(0x20u, 0xFFFFFFF0u, 49u));
    assert(meshControlTimedOut(0x21u, 0xFFFFFFF0u, 49u));

    MeshFxActivity activity;
    assert(!activity.active());
    assert(!activity.timedOut(timeoutMs, timeoutMs));
    activity.record(1000u);
    assert(activity.active());
    assert(!activity.timedOut(1000u + 7u * 60u * 1000u, timeoutMs));
    activity.record(1000u + 7u * 60u * 1000u);
    assert(!activity.timedOut(1000u + 14u * 60u * 1000u, timeoutMs));
    activity.record(1000u + 14u * 60u * 1000u);
    assert(!activity.timedOut(1000u + 20u * 60u * 1000u, timeoutMs));
    assert(activity.timedOut(1000u + 34u * 60u * 1000u, timeoutMs));
    activity.clear();
    assert(!activity.active());
    assert(!activity.timedOut(1000u + 40u * 60u * 1000u, timeoutMs));

    activity.record(0xFFFFFFF0u);
    assert(!activity.timedOut(0x20u, 49u));
    assert(activity.timedOut(0x21u, 49u));
    return 0;
}
