---
type: Issue
title: Cross-task effect registry access
description: setupAlarmSchedule() runs on the ALM task (CORE0) and calls adjustCurrentEffect(), which reaches fxRegistry.setSleepState(), transitionEffect() and FastLED.show() while the FX task on CORE1 owns the registry and the LED buffers. The registry has no lock.
tags: [risk, concurrency, race, registry, freertos]
severity: medium
issue_state: fixed
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: sched
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/FxSchedule.cpp
    title: src/FxSchedule.cpp (setupAlarmSchedule)
  - id: fxutil
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/fxutil.cpp
    title: src/fxutil.cpp (adjustCurrentEffect)
  - id: reg
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/EffectRegistry.cpp
    title: src/EffectRegistry.cpp
---

# Path

ALM task → `alarm_setup()` / `alarm_check()` → `setupAlarmSchedule()` → `adjustCurrentEffect(now)` → `fxRegistry.setSleepState(...)` → `nextEffectPos(...)` → `transitionEffect()`.[^sched][^fxutil]

When the sleep state **changes**, `transitionEffect()` on the ALM task does the following:

* writes `nextEffectIndex` and `desiredEffectIndex`, and calls `activeEffect->desiredState(Idle)`;
* blends `ledSet` and calls `FastLED.show()`;
* calls `transEffect.prepare()` and posts `FX_SYNC`.[^reg]

At the same moment the FX task may be inside `fxRegistry.loop()`, which can **delete `activeEffect`** or render into `leds`. The registry mutex was removed in `3b2aa4d` on the assumption that only the FX task touches the registry. This path breaks that assumption.

# When it can happen

The race is reachable only when `setSleepState` sees a real change:

* At boot, when the first `ALARM_SETUP` runs while the time is inside the sleep window and sleep is enabled.
* After a clock jump or reschedule, if the computed sleep state differs from the current one.

Regular bedtime and wake-up go through `fxQueue` (`SLEEP_STATE`) and are safe.

# Possible consequences

Use-after-free on `activeEffect`, a torn transition, or two `FastLED.show()` calls from different cores at once (a PIO/DMA conflict). These are rare, but they fit the profile of the "FX stall" and hard-fault reboots that the [watchdog markers](/architecture/watchdog-and-health.md) were added to investigate.

# Fix

Have `adjustCurrentEffect` post `FxActionMessage{SLEEP_STATE, asleep}` to `fxQueue` instead of calling the registry directly. `wakeup()` and `bedtime()` already do this. `fx_run` already handles `SLEEP_STATE`.

# Resolution

Fixed in `d5de0ba` as described above: `adjustCurrentEffect` computes the sleep state and posts `SLEEP_STATE` to `fxQueue`; the FX task applies it on CORE1.

# Verification status

The call path is confirmed in the source. That it causes the observed crashes is **plausible**, not proven. The fix compiles; not yet tested on a board.

[^sched]: src/FxSchedule.cpp (setupAlarmSchedule)
[^fxutil]: src/fxutil.cpp (adjustCurrentEffect)
[^reg]: src/EffectRegistry.cpp
