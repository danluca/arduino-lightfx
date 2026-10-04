---
type: Architecture
title: Time sync
description: NTP time from a local NTP server, America/Chicago zone rules, a re-sync every 17 h with a 12-hour minimum, a 60-second retry until the first success, and drift correction.
tags: [time, ntp, timezone, dst, drift]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: time
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/timeutil.cpp
    title: src/timeutil.cpp
  - id: comms
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/comms.cpp
    title: src/comms.cpp (timeUpdate, timeSetupCheck)
  - id: config
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/config.h
    title: include/config.h (NTP_SERVER_IP)
---

# Sources, in priority order

1. **NTP** through `TimeService` (`lib/TimeLib`), over a `WiFiUDP` that is created lazily in `timeBegin()`. With `-D LOCAL_NTP_SERVER=1` (the default in both envs) it queries `192.168.0.58` (the house NAS). Otherwise it uses `pool.ntp.org`. A local NTP server exists because the simple UDP client does not get through strict networks reliably.[^config]
2. **On failure, `handleNTPFailure()`** clears `Ntp`, logs an error and keeps the unsynchronized local clock and the saved holiday. There is no second time source: on this board the arduino-pico `WiFi.getTime()` returns `millis()`, so it is deliberately not used (unlike the RP2040 line's NINA module). See [Wi-Fi time fallback](/issues/wifi-gettime-fallback.md).

# Time zone

Hard-coded **America/Chicago**: CDT (UTC−5) from the second Sunday of March at 02:00, and CST (UTC−6) from the first Sunday of November at 02:00. `SysStatus::Dst` tracks the current state, and `timeUpdate` logs any change.[^time]

# Cadence

| Event | When | What |
|---|---|---|
| `timeSetup()` | Boot, after Wi-Fi | NTP. On success: set `Ntp`/`Dst`, adjust the holiday, re-base the log timestamps, record a `TimeSync`, and fix timestamps captured before sync (watchdog reboots, calibration). On failure: `handleNTPFailure()` |
| `timeSetupCheck` | One-shot timer, 60 s, repeated until NTP is OK | Retries `timeSetup()`. Enqueues `ALARM_SETUP` on success |
| `timeUpdate` | Every 17 h | Re-sync, skipped if the last sync was less than 12 h ago. On the first success it enqueues `ALARM_SETUP`. Applies drift |
| `wifi_ensure` | Every 7 min | Calls `postTimeSetupCheck()`, which restarts the 60 s retry timer if NTP is still missing |

# Drift

`timeSyncs` (a FixedQueue of 8) stores `{localMillis, unixMillis}` pairs. On each re-sync, `drift = Δlocal − Δunix`. If |drift| ≤ 1 h, `timeService.addDrift(-drift)` corrects the time base. Larger values are logged and ignored. `/status.json` → `time` exposes `averageDrift` (ms/h), `lastDrift`, `totalDrift`, `currentDrift` and the raw `syncs`.[^comms]

The log time base is re-anchored whenever it is more than 5 minutes off from real time.

[^time]: src/timeutil.cpp
[^comms]: src/comms.cpp (timeUpdate, timeSetupCheck)
[^config]: include/config.h (NTP_SERVER_IP)
