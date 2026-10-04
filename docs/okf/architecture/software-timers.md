---
type: Reference
title: Software timers
description: Every FreeRTOS software timer, its period and type, the message it enqueues, and the task that handles it.
tags: [freertos, timers, scheduling]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: comms
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/comms.cpp
    title: src/comms.cpp (commSetup, startTimeSetupTimer)
  - id: diag
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/diag.cpp
    title: src/diag.cpp (diagSetup)
  - id: sched
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/FxSchedule.cpp
    title: src/FxSchedule.cpp (alarm_setup, alarm_check)
---

# Timers

| Timer (id) | Period | Type | Created in | Posts | Handled by |
|---|---|---|---|---|---|
| `rndEntropy` (10) | 6 min | repeat | `diagSetup` | `diagQueue` `RND_ENTROPY` | CORE1: hardware RNG → `random16_add_entropy` |
| `sysTemp` (11) | 32 s | repeat | `diagSetup` | `diagQueue` `SYS_TEMP` | CORE1: CPU temperature, calibration |
| `sysVoltage` (12) | 34 s | repeat | `diagSetup` | `diagQueue` `SYS_VOLTAGE` | CORE1: A0 supply voltage |
| `saveSysInfo` (13) | 317 s | repeat | `diagSetup` | `almQueue` `SAVE_SYS_INFO` | ALM: write `/sys.json` and `/status/sysconfig.json` |
| `diagInfo` (14) | 30.25 s | repeat | `diagSetup` (only if `LOGGING_ENABLED`) | `diagQueue` `DIAG_INFO` | CORE1: task summary log |
| `fxHeartbeat` (15) | 3 s | repeat | `diagSetup` | `diagQueue` `FX_HEARTBEAT` | CORE1: FX stall detection |
| `taskSnapshot` (16) | 10 s | repeat | `diagSetup` | `diagQueue` `TASK_SNAPSHOT` | CORE1: capture per-task run-time stats for `/tasks.json` (runs even without logging) |
| `timeUpdate` (20) | 17 h | repeat | `commSetup` | `bcQueue` `TIME_UPDATE` | CORE0: NTP re-sync and drift correction |
| `wifiEnsure` (21) | 7 min | repeat | `commSetup` | `bcQueue` `WIFI_ENSURE` | CORE0: ping the gateway, check RSSI, reconnect |
| `statusLEDCheck` (23) | 5 s | repeat | `commSetup` | `bcQueue` `STATUS_LED_CHECK` | CORE0: on-board RGB LED |
| `timeSetup` (24) | 60 s | one-shot, reset on reuse | `startTimeSetupTimer` | `bcQueue` `TIME_SETUP` | CORE0: retry NTP until the first success |
| `scanClients` (25) | 15 min | repeat | `commSetup` | `bcQueue` `SCAN_CLIENTS` | CORE0: merge mDNS-discovered boards, ping broadcast recipients (mDNS builds) |
| `alarmCheck` (30) | dynamic | one-shot, re-armed | `alarm_setup` / `alarm_check` | `almQueue` `ALARM_CHECK` | ALM: fire due wake/bed alarms |
| `holidayUpdate` (31) | 12 h | repeat | `alarm_setup` | `almQueue` `HOLIDAY_UPDATE` | ALM: re-derive the holiday from the date |

Id 22 is unused on this board. The RP2040 line uses it for the NINA Wi-Fi chip temperature timer.

The `alarmCheck` period is `max(min(next_alarm - now, 12 h) / 10, 60 s)`, which works out to between 1 and 72 minutes.[^sched]

# Notes

* Timer IDs point at `static uint16_t` variables. That is how `getTimerId` recovers the numeric id.
* Periods are deliberately offset from each other (32/34 s, 317 s), so periodic ADC and filesystem work do not line up.
* `alarmCheck` is created once and then re-armed with `xTimerChangePeriod` and `xTimerReset`. An earlier version leaked a new timer on every check.[^sched]
* The FX task does its own periodic work with FastLED `EVERY_N_*` macros instead of timers: a brightness check every 30 s, and a random effect switch every 7 min. See [effect registry](/effects/effect-registry.md).
* `configTIMER_QUEUE_LENGTH=24` leaves room for the commands of these 14 timers.

[^comms]: src/comms.cpp (commSetup, startTimeSetupTimer)
[^diag]: src/diag.cpp (diagSetup)
[^sched]: src/FxSchedule.cpp (alarm_setup, alarm_check)
