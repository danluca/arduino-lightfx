---
type: Architecture
title: Time sync
description: The system clock is set by the lwIP SNTP client from one NTP server, polled every 12 h. Local time uses the C library with a POSIX TZ string (America/Chicago). An hourly housekeeping task reconciles the NTP and DST flags, and a 60-second timer retries until the first sync.
tags: [time, ntp, sntp, timezone, dst]
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
  - id: timelib
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/lib/TimeLib/src/TimeService.cpp
    title: lib/TimeLib/src/TimeService.cpp
---

# Design

The firmware uses the platform primitives and keeps no clock logic of its own (no drift tracking, no sync history, no zone rules in code).

| Concern | Provided by |
|---|---|
| UTC clock | The system clock behind `gettimeofday()` (`cores/rp2040/posix.cpp`: boot-relative time plus a delta). There is no battery-backed RTC, so it restarts from 1970 at every boot |
| NTP | The lwIP SNTP client, started through the framework's `NTP` object (`NTP.begin(IPAddress)`). It polls in the background and steps the system clock |
| Local time | newlib `localtime_r`/`mktime`, driven by a POSIX `TZ` string installed once at boot (`setenv` + `tzset`) |
| Zone rules | `TZ_America_Chicago` from the framework's `TZ.h` (`"CST6CDT,M3.2.0,M11.1.0"`) |

`lib/TimeLib` is a thin facade over these: `TimeService` (system clock, NTP control, sync status), `Timezone` (wraps the TZ string), and the `now()`, `hour()`, `TimeFormat` helpers, whose API did not change. `now()` is still **local** seconds since 1970; `utcNow()` is UTC.[^timelib]

# NTP

1. **Server.** One address, no fallback. With `-D LOCAL_NTP_SERVER=1` (the default in both envs) it is `NTP_SERVER_IP` in `config.h` (`192.168.0.58`, the house NAS), given as an IP address so no DNS lookup is needed. Without the flag, `timeBegin()` resolves `pool.ntp.org` once.[^config]
2. **Poll interval: 12 hours** (`SNTP_UPDATE_DELAY=43200000UL`). lwIP retries failures on its own with a growing delay. The lwIP SNTP code is part of the framework's lwIP and sees the **framework's** `lwipopts.h`, not `include/lwipopts.h` (see [framework customizations](/build/framework-customizations.md)), so the interval is a build flag in `platformio.ini`, not an `lwipopts.h` entry.
3. **Validation hook.** The same file adds `-Wl,--wrap=__setSystemTime`. The SNTP client's call that sets the clock lands in `__wrap___setSystemTime` (`TimeService.cpp`), which rejects times outside 2020-01-01..2070-01-01, calls the framework's real function otherwise, and records the sync count and time. It runs in the lwIP task: no logging, no blocking calls. `TimeService::setTime()` calls the real function directly, so manual sets do not count as syncs.
4. **Clock step.** lwIP sets the clock in one step (no slew). Between syncs the clock runs on the Pico crystal and drifts by seconds per day, which is fine for dimming and holiday logic.

# Time zone

`America/Chicago`, from the POSIX rule above: CDT (UTC−5) from the second Sunday of March at 02:00, CST (UTC−6) from the first Sunday of November at 02:00. `Timezone` computes offset, DST flag and abbreviation from `localtime_r`/`mktime`/`tzname`; newlib guards them with its retargeted locks. `SysStatus::Dst` tracks the current state, and `timeUpdate` logs any change.[^time]

Limits, by design: one standard offset and one DST rule per zone, no historical rule changes, and a rule change needs a firmware update. Only one zone can be active, because the C library keeps it in a process-wide variable. To move a board to another zone, change `TZ_America_Chicago` (and the name) in `timeutil.cpp` to another `TZ_*` macro from `TZ.h`.

The zone is applied in `timeSetup()` on the first call, not earlier. Timestamps captured before that (watchdog reboots, temperature calibration) are boot-relative seconds; on the first sync `handleNTPSuccess()` converts them with `TimeService::utcFromRtcMillis()`.

# Cadence

| Event | When | What |
|---|---|---|
| `timeSetup(NTP_BOOT_WAIT_MS)` | Boot, after Wi-Fi | Applies the zone, starts the SNTP client and waits up to 10 s for the first sync. On success: set `Ntp`/`Dst`, adjust the holiday, re-base the log timestamps, fix timestamps captured before sync. On failure: `handleNTPFailure()` |
| `timeSetupCheck` | One-shot timer, 60 s, repeated until the first sync | Calls `timeSetup()` without waiting. It only checks whether the SNTP client has delivered a time (it does not trigger a query). Enqueues `ALARM_SETUP` on success |
| `timeUpdate` | Every 1 h | Housekeeping only. Makes sure the SNTP client is running. If `Ntp` is not set it takes the retry path. If it is set, it clears `Ntp` when the last sync is older than 36 h (`NTP_STALE_SECS`), and otherwise re-anchors the log time base and checks for a DST transition |
| `wifi_ensure` | Every 7 min | After a Wi-Fi reconnect, restarts the SNTP client (which queries right away) through `timeBegin()`, then calls `postTimeSetupCheck()` |

`wifi_reconnect()` stops the SNTP client before tearing down Wi-Fi.

# On failure

`handleNTPFailure()` clears `Ntp`, logs an error and keeps the unsynchronized clock and the saved holiday. There is no second time source: on this board the arduino-pico `WiFi.getTime()` returns `millis()`, so it is deliberately not used (unlike the RP2040 line's NINA module). See [Wi-Fi time fallback](/issues/wifi-gettime-fallback.md).

# Status

`/status.json` → `time` exposes `ntpSync`, `dst`, `zoneDST`, `offset`, `zone`, `zoneShort`, `syncCount`, `syncRejected`, `lastSync` (UTC unix seconds, 0 if none) and `lastSyncAge` (seconds, −1 if none). The drift fields of the earlier design are gone.

# Verification status

Built for `rp2350-rel`. The compiled SNTP object holds the 12-hour interval, and in the linked firmware its clock-set call goes to `__wrap___setSystemTime`. Not yet run on a board: the NTP exchange with the NAS, the clock step, the zone conversions around the 2026-03-08 and 2026-11-01 transitions, and the 12-hour re-poll still need checking on hardware.

[^time]: src/timeutil.cpp
[^comms]: src/comms.cpp (timeUpdate, timeSetupCheck)
[^config]: include/config.h (NTP_SERVER_IP)
[^timelib]: lib/TimeLib/src/TimeService.cpp
