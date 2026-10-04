---
type: Business Rule
title: Sleep and wake schedule
description: The daily bedtime (00:30) and wake-up (06:00) alarms. They are scheduled on the ALM task and switch the registry to and from the FXA6 sleep light when sleep is enabled.
tags: [sleep, alarms, schedule, alm]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: sched
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/FxSchedule.cpp
    title: src/FxSchedule.cpp
  - id: efx
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/efx_setup.cpp
    title: src/efx_setup.cpp (wakeup, bedtime)
---

# Constants

* `dailyBedTime = 00:30` (30 min after midnight)
* `dailyWakeupTime = 06:00`

Both are compile-time constants. Nothing at runtime can change them.[^sched]

# Mechanism

1. `ALARM_SETUP` is enqueued after the first successful NTP sync: during boot, or later from `timeUpdate` or `timeSetupCheck`. The ALM task then runs `alarm_setup()`.
2. `scheduleDay(now)` makes sure there is one upcoming `WAKEUP` and one upcoming `BEDTIME` alarm within the next 48 h. If today's instant has already passed, it schedules tomorrow's. Alarms are heap-allocated `AlarmData` objects kept in a static `std::deque`, guarded by `almMutex`. `/status.json` reads them through `getScheduledAlarmsCopy()`, which copies under the lock.
3. `adjustCurrentEffect(now)` computes the right sleep state for the current time and posts it to `fxQueue` as `SLEEP_STATE`, so the FX task applies it.
4. A one-shot `alarmCheck` timer re-arms itself at `clamp((next - now)/10, 60 s, 72 min)`. Each check removes due alarms under the lock and calls their handlers **after** releasing it. It reschedules on a new day, when the queue is empty, or when the next alarm is more than 24 h away (which means the clock jumped).
5. Handlers: `wakeup()` posts `SLEEP_STATE 0` to `fxQueue`. `bedtime()` posts `SLEEP_STATE 1`, but only if sleep is enabled.[^efx]

# Effect on lights

* `SLEEP_STATE 1` switches to **FXA6 Sleep Light** and remembers the previous effect. `SLEEP_STATE 0` restores it. While asleep, auto-roll, audio bumps and the 7-minute random switch do nothing.
* `isAwakeTime(t)` handles both orderings of bedtime and wake-up time (with or without midnight in between).
* The sleep schedule is **off by default**. Turn it on from the web UI ("Sleep schedule enabled") or with `PUT /fx {"sleepEnabled":true}`. The setting is saved in `/state.json` and restored on boot.

# Where to look when it misbehaves

* `GET /status.json` → `time.alarms[]` lists the pending alarms with their formatted times. `fx.sleepEnabled` and `fx.asleep` show the state.
* Without Wi-Fi at the time, `setupAlarmSchedule()` logs "Cannot setup alarms without WiFi" and schedules nothing until the next successful NTP sync.

[^sched]: src/FxSchedule.cpp
[^efx]: src/efx_setup.cpp (wakeup, bedtime)
