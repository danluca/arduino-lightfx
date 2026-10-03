---
type: Reference
title: System status flags
description: The SysStatus bitmask (Wi-Fi, NTP, DST and others) that gates behavior across tasks, and the on-board RGB status LED colors derived from it.
tags: [status, flags, led, diagnostics]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/include/util.h
    title: include/util.h (SysStatus)
  - id: sysinfo
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/sysinfo.cpp
    title: src/sysinfo.cpp (status LED, isSysStatus)
---

# Bits

| Flag | Mask | Set when | Cleared when |
|---|---|---|---|
| `Setup0` | 0x0001 | CORE0 `setup()` finishes | never |
| `Setup1` | 0x0002 | CORE1 `setup1()` finishes | never |
| `Filesystem` | 0x0004 | LittleFS mounted through `SyncFsImpl` | never |
| `Wifi` | 0x0008 | Wi-Fi connected | `wifi_check` fails (no gateway ping, or RSSI < −75 dBm), or `wifi_reconnect` |
| `Ntp` | 0x0010 | NTP or Wi-Fi time obtained | an NTP sync fails |
| `Dst` | 0x0020 | Central Time is in DST | DST ends |
| `Diag` | 0x0040 | ADC, IMU and calibration ready | never |
| `Mic` | 0x0080 | PDM microphone started | never |
| `Ecc` | 0x0100 | ECC608 secure element is present and locked | never |

`isSysStatus(mask)` returns true only if **all** bits in `mask` are set. Access goes through a `CoreMutex`, so any task or core can call it. `GET /status.json` exposes the raw value as `overallStatus`, and the logs print it as `%#hX`.[^sysinfo]

# What the flags gate

* Without `Wifi`: holiday auto-detection keeps the saved holiday, alarms are not scheduled, time-of-day dimming is skipped, and comms and NTP calls bail out early.
* Without `Ntp`: `resetGlobals` does not flush the cleared strip, and `ALARM_SETUP` is deferred until a later NTP success.
* Without `Ecc`: `secRandom*` falls back to the Arduino `random()`.

# Status LED (on-board RGB, driven through the NINA module)

| Color | Meaning |
|---|---|
| Blinking green (640 ms) | Boot in progress: `Setup0` and `Setup1` are not both set yet (`state_led_begin` on ALM) |
| Indigo | All OK: `Wifi`, `Ntp`, `Filesystem` and `Diag` are all set |
| Green | Setup still in progress |
| Red | Setup finished, but at least one of the OK flags is missing |

The LED updates every 5 s through `STATUS_LED_CHECK` on CORE0. It has to stay on the Wi-Fi task, because the LED pins belong to the NINA module.[^sysinfo]

[^util]: include/util.h (SysStatus)
[^sysinfo]: src/sysinfo.cpp (status LED, isSysStatus)
