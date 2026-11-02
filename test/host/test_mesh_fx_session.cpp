#include <cassert>
#include <cstdint>
#include "mesh_fx_session.h"

int main() {
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