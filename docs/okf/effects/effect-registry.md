---
type: Architecture
title: Effect registry
description: The global EffectRegistry. It registers effects, assigns registry indexes, picks the next effect by holiday-weighted random selection, handles auto-roll and sleep, and creates each new effect only after the old one is deleted.
tags: [effects, registry, selection, sleep, random]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: regh
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/EffectRegistry.h
    title: include/EffectRegistry.h
  - id: regcpp
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/EffectRegistry.cpp
    title: src/EffectRegistry.cpp
  - id: efx
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/efx_setup.cpp
    title: src/efx_setup.cpp (fx_run)
---

# Registration

* Each category file declares `static const EffectInfo` objects. An `EffectInfo` holds a factory lambda (`EFFECT_FACTORY(Class)`), `{id, description}`, a default `selectionWeight` (0–255), and an optional `HOLIDAY_WEIGHTS(array)`. The category's `fxRegister()` pushes them into `fxRegistry`.
* The **registry index** is the registration order: categories A→K, and within each file the order of `registerEffect` calls. The web UI and broadcast sync identify effects **by index**, so reordering registrations changes which effect a stored or broadcast index refers to. See [effect catalog](/effects/effect-catalog.md).
* The effect whose ID equals `FX_SLEEPLIGHT_ID` (`"FXA6"`) becomes the **sleep effect**. Random selection and sequential stepping skip it.[^regcpp]

# Deferred creation (anti-fragmentation)

Only one `LedEffect` instance exists at any time.[^regcpp]

1. `transitionEffect()` runs when the desired index differs from the current one. It sets `nextEffectIndex` and asks the active effect for `desiredState(Idle)`. It blends the strip toward a palette color, calls `transEffect.prepare(random8())`, and posts `FX_SYNC` for [broadcast](/network/multi-board-broadcast.md).
2. Each `loop()` advances the active effect. Once the effect reaches `Idle`, the registry **deletes** it, records the index in a 20-entry history, and only then calls the factory for `nextEffectIndex`, setting `desiredState(Running)`.

# Selection modes

| Trigger | Method | Behavior |
|---|---|---|
| Every 7 min (`EVERY_N_MINUTES(7)` in `fx_run`) | `nextRandomEffectPos()` | Holiday-weighted random pick, then reshuffle the strip indexes, then mark the state dirty so `fx_run` saves `/state.json` |
| Web or broadcast `MANUAL_FX` | `nextEffectPos(uint16_t)` | Jump to the index, capped at `size-1`. Ignores auto-roll and sleep |
| Sleep on/off | `setSleepState(bool)` | Switch to FXA6 and remember the previous effect, or restore it |

The RP2040 line also has a sequential `nextEffectPos()` for microphone bumps; it was removed here.

Random and sequential selection do nothing when **auto-roll is off** or the registry **is asleep**.

# Holiday-weighted random selection

The selection weight is `effectiveSelectionWeight(holiday)`: the holiday override if one exists, otherwise the default weight. A weight of 0 excludes the effect. The algorithm draws `rnd = random16(total)` and walks the cumulative weights, skipping the sleep effect. If the total is 0, it keeps the current effect and logs a warning. This feature was added in `6a6eba9` ("added dynamic weighting by holiday").[^regcpp] An off-by-one in the first version (`random16(total+1)`) is fixed. See [weighted-random off-by-one](/issues/weighted-random-off-by-one.md).

# Sleep

* `enableSleep(bool)` sets whether the sleep schedule is honored, then immediately evaluates `setSleepState(enabled && !isAwakeTime(now))`.
* `setSleepState(true)` jumps to FXA6 and stores `beforeSleepEffectIndex`. `false` restores that index.
* `restoreDesiredEffectFromState(fx)` runs on boot. If asleep it picks FXA6. If the saved effect *was* FXA6 while awake, it picks a random index. Otherwise it uses the saved index.
* See [sleep and wake schedule](/scheduling/sleep-wake-schedule.md).

# Thread-safety note

A registry mutex was removed in `3b2aa4d` (2026-03-23) because, by design, only the FX task touches the registry. `adjustCurrentEffect()` used to break that assumption from the ALM task; it now posts `SLEEP_STATE` to `fxQueue` instead. The web and comms handlers on CORE0 still call read-only accessors (`curEffectPos`, `getEffectInfo`, `isAutoRoll`, `pastEffectsRun`) without a lock. See [cross-task registry access](/issues/cross-task-registry-access.md).

# Lookup gotchas

* `findEffectIndex(id)` and `nextEffectPos(const char*)` return **0 when the ID is not found**, which cannot be told apart from FXA1 (index 0).
* IDs are compared case-sensitively. One ID is `FxC4`, not `FXC4`. See [effect ID inconsistency](/issues/effect-id-case.md).

[^regh]: include/EffectRegistry.h
[^regcpp]: src/EffectRegistry.cpp
[^efx]: src/efx_setup.cpp (fx_run)
