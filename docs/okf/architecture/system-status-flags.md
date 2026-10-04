---
type: Reference
title: System status flags
description: The SysStatus bitmask (Wi-Fi, NTP, DST and others) that gates behavior across tasks, and the on-board RGB status LED colors derived from it.
tags: [status, flags, led, diagnostics]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/util.h
    title: include/util.h (SysStatus)
  - id: sysinfo
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/sysinfo_state.cpp
    title: src/sysinfo_state.cpp (isSysStatus)
  - id: led
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/sysinfo_led.cpp
    title: src/sysinfo_led.cpp (status LED)
---

# Bits

| Flag | Mask | Set when | Cleared when |
|---|---|---|---|
| `Setup0` | 0x0001 | CORE0 `setup()` finishes | never |
| `Setup1` | 0x0002 | CORE1 `setup1()` finishes | never |
| `Filesystem` | 0x0004 | LittleFS mounted through `SyncFsImpl` | never |
| `Wifi` | 0x0008 | Wi-Fi connected, or `wifi_check` passes | `wifi_check` fails (not connected, no gateway ping, or RSSI < −75 dBm), or `wifi_reconnect` |
| `Ntp` | 0x0010 | NTP time obtained | an NTP sync fails |
| `Dst` | 0x0020 | Central Time is in DST | DST ends |
| `Diag` | 0x0040 | ADC and calibration ready (`deviceSetup`) | never |

The RP2040 line also has `Mic` (0x0080) and `Ecc` (0x0100). Neither exists here, because the Plasma 2350 W has no microphone and no secure element.[^util]

`isSysStatus(mask)` returns true only if **all** bits in `mask` are set. Access goes through a mutex, so any task or core can call it. `GET /status.json` exposes the raw value as `overallStatus`, and the logs print it as `%#hX`. A fully healthy board reports `0x7F`, or `0x5F` outside DST.[^sysinfo]

# What the flags gate

* Without `Wifi`: holiday auto-detection keeps the saved holiday, alarms are not scheduled, time-of-day dimming is skipped, and comms and NTP calls bail out early.
* Without `Ntp`: `resetGlobals` does not flush the cleared strip, and `ALARM_SETUP` is deferred until a later NTP success.

# Status LED (on-board RGB)

The Plasma 2350 W's RGB LED is driven directly from GPIO 16 (R), 17 (G) and 18 (B) with `analogWrite`. The LED is active-low, so the code writes `255 - value`.[^led]

| Color | Meaning |
|---|---|
| Blinking green (640 ms) | Boot in progress: `Setup0` and `Setup1` are not both set yet (`state_led_begin` on ALM) |
| Indigo | All OK: `Wifi`, `Ntp`, `Filesystem` and `Diag` are all set |
| Green | Setup still in progress |
| Red | Setup finished, but at least one of the OK flags is missing |

The LED updates every 5 s through `STATUS_LED_CHECK` on CORE0. Because the LED is a plain GPIO here, unlike the NINA-driven LED on the RP2040 line, any task could update it; the update still runs on CORE0 to keep a single writer.[^led]

[^util]: include/util.h (SysStatus)
[^sysinfo]: src/sysinfo_state.cpp (isSysStatus)
[^led]: src/sysinfo_led.cpp (status LED)
