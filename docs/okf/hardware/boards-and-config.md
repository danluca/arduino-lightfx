---
type: Configuration
title: Boards and configuration
description: Per-board runtime configuration selected from a bundled UID table, covering pixel counts, frame sizes, network identity, measured voltages and LED wiring.
tags: [config, boards, hardware, deployment]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: config
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/config.h
    title: include/config.h
  - id: boards
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/scripts/boards.ps1
    title: scripts/boards.ps1
  - id: variant
    resource: https://github.com/earlephilhower/arduino-pico/blob/master/variants/pimoroni_plasma2350w/pins_arduino.h
    title: arduino-pico variants/pimoroni_plasma2350w/pins_arduino.h
---

# Boards

The firmware selects hardware settings at boot from the UID table in `src/board_config.cpp`; `-board` in the build/update scripts only selects a deployment target. Dev and Tree use different LED protocols, and the actual flash UIDs still need to be recorded in the profile slots.

| UID slot | Device name | IP mode | LED chipset / order | Pixels | Frame | Calibration |
|---|---|---|---|---:|---:|---|
| `LIGHTFX_DEV_BOARD_UID` (`SET_DEV_UID`) | `Xmas2350` | DHCP | `WS2812B` / `BGR` | 66 | 22 | 3317 mV; R4/R5 22000/4700 Ω; 10 m Pimoroni LED star wire |
| `LIGHTFX_TREE_BOARD_UID` (`SET_TREE_UID`) | `FXPine` | DHCP | `WS2811` / `RGB` | 1000 | 75 | 3317 mV; R4/R5 22000/4700 Ω; blue pine tree lighting |

Replace both placeholder UID strings with the boards' actual flash UIDs before deployment. Until configured, those boards safely use the unknown-board profile. Unknown UIDs get the hostname identity `New-` plus their last six UID characters, DHCP, 32 pixels, a 14-pixel frame, conservative 3300 mV and R4/R5 20000/3300 Ω calibration, and a 16/255 brightness cap. LED output and effect scratch buffers are allocated to the selected geometry at startup; geometries must provide at least 14 frame pixels and two frames must fit within the strip. CPU temperature calibration saved with a different `mv3_3` reference is discarded.

# Addresses

| `-board` | DHCP reservation | mDNS host when UID is configured |
|---|---|---|
| Dev | 192.168.0.75 | `lightfx-xmas2350.local` |
| Tree | 192.168.0.182 | `lightfx-fxpine.local` |

Both profiles use **DHCP** with a router reservation per MAC; no static address is applied by the firmware. Until the UIDs are configured, their runtime mDNS names use the `lightfx-new-<last-six-uid-characters>.local` form. The scripts talk to the `boards.ps1` reservation address (and `scripts/upload_audio_seed.sh` to the Dev address), so keep the script entries and router reservations in step.

# Shared settings

| Setting | Value |
|---|---|
| `LED_PIN` | `PIN_NEOPIXEL` = GPIO 15, the Plasma 2350 W's LED data output[^variant] |
| Status LED | RGB on GPIO 16/17/18 (`PIN_LED_R/G/B`), active-low |
| `BRIGHTNESS` | 255 (the global FastLED brightness; time-of-day dimming is applied through `stripBrightness`) |
| Supported geometry | At most 1024 LEDs and a 256-pixel frame (operational limits, not preallocated array capacities) |
| Gateway / DNS | 192.168.0.1 |
| `STATIC_BROADCAST_CLIENTS` | 10, 11, 12 |
| `NTP_SERVER_IP` | 192.168.0.58 (when `LOCAL_NTP_SERVER` is defined) |
| `MDNS_CACHING_TIMEOUT_MS` | 60 min |
| `IGNORE_WEB_EFFECT_CHANGES` | 0 by default |

# Adding a board

1. Record the board’s `rp2040.getChipID()` value and replace the matching `LIGHTFX_*_BOARD_UID` placeholder in `src/board_config.cpp`; set the board's name, geometry, chipset, color order and calibration there.
2. Add its logical name and target address to `$boardMap` in `scripts/boards.ps1` and to the relevant script `ValidateSet` lists.
3. Add its last IP octet to `STATIC_BROADCAST_CLIENTS` if it should take part in [broadcast](/network/multi-board-broadcast.md).

[^config]: include/config.h
[^boards]: scripts/boards.ps1
[^variant]: arduino-pico variants/pimoroni_plasma2350w/pins_arduino.h
