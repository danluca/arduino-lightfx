---
type: Configuration
title: Boards and configuration
description: Per-board runtime configuration selected from a bundled UID table, covering pixel counts, frame sizes, network identity, measured voltages and LED wiring.
tags: [config, boards, hardware, deployment]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: config
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/include/config.h
    title: include/config.h
  - id: boards
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/scripts/boards.ps1
    title: scripts/boards.ps1
---

# Boards

The firmware selects hardware settings at boot from the static UID table in `src/board_config.cpp`; `-board` in the build/update scripts only selects a deployment target. All profiles use the same physical Nano RP2040 Connect target.

| UID | `DEVICE_NAME` | IP mode | `NUM_PIXELS` | `FRAME_SIZE` | Calibration |
|---|---|---|---:|---:|---|
| `E034C6063295681A` | `FX02-Office` | DHCP | 170 | 76 | 3262 mV; R4/R5 19890/3302 Ω |
| `5034C60632956818` | `Dev-Office` | DHCP | 170 | 76 | 3262 mV; R4/R5 21950/3304 Ω |

The unknown-board fallback is named from the last six UID characters and uses DHCP, 32 pixels, a 12-pixel frame, conservative 3300 mV and R4/R5 20000/3300 Ω calibration, and a 16/255 brightness cap. The FX01 UID has not yet been recorded, so that board currently uses these safe defaults. LED output and effect scratch buffers are allocated to the selected geometry at startup; CPU temperature calibration saved with a different `mv3_3` reference is discarded.

# Shared settings

| Setting | Value |
|---|---|
| `LED_PIN` | 25 (D2 on the Nano RP2040 Connect) |
| `CHIPSET` / `COLOR_ORDER` | `WS2811` / `BRG` |
| `BRIGHTNESS` | 255 (the global FastLED brightness; time-of-day dimming is applied through `stripBrightness`) |
| Supported geometry | At most 320 LEDs and a 76-pixel frame (operational limits, not preallocated array capacities) |
| Gateway / DNS | 192.168.0.1 |
| `STATIC_BROADCAST_CLIENTS` | 10, 11, 12 |
| `NTP_SERVER_IP` | 192.168.0.58 (when `LOCAL_NTP_SERVER` is defined) |
| `MDNS_CACHING_TIMEOUT_MS` | 60 min |
| `IGNORE_WEB_EFFECT_CHANGES` | 0 by default |

# Adding a board

1. Record the board’s `rp2040.getChipID()` value and add a validated entry to `boardConfigurations` in `src/board_config.cpp`.
2. Add its logical name and target address to `$boardMap` in `scripts/boards.ps1` and to the relevant script `ValidateSet` lists.
3. Add its last IP octet to `STATIC_BROADCAST_CLIENTS` if it should take part in [broadcast](/network/multi-board-broadcast.md).

[^config]: include/config.h
[^boards]: scripts/boards.ps1
