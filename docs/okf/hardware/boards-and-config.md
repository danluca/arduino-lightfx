---
type: Configuration
title: Boards and configuration
description: Per-board compile-time configuration selected with BOARD_ID (1 Dev, 2 FX01 house, 3 FX02 office), covering pixel counts, frame sizes, static IPs, measured voltages and LED wiring.
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

`BOARD_ID` defaults to 1. The build scripts set it through `PLATFORMIO_BUILD_FLAGS=-DBOARD_ID=<id>` based on `-board`.[^boards]

| `-board` | `BOARD_ID` | `DEVICE_NAME` | IP | `NUM_PIXELS` | `FRAME_SIZE` | Installation |
|---|---:|---|---|---:|---:|---|
| Dev | 1 | `Dev-Office` | 192.168.0.10 | 170 | 76 | Development board, office window edge (166 px) |
| FX01 | 2 | `FX01-House` | 192.168.0.11 | 320 | 68 | House edge (300 px measured, plus a reserve) |
| FX02 | 3 | `FX02-Office` | 192.168.0.12 | 170 | 76 | Spare and office controller |

`PIXEL_BUFFER_SPACE = 4 × FRAME_SIZE` for every board. Each board also defines its measured 3V3 rail (`V3_3`, `MV3_3`) and the A0 voltage divider resistors (`VCC_DIV_R4`, `VCC_DIV_R5`), which are used for supply voltage readings.[^config]

# Shared settings

| Setting | Value |
|---|---|
| `LED_PIN` | 25 (D2 on the Nano RP2040 Connect) |
| `CHIPSET` / `COLOR_ORDER` | `WS2811` / `BRG` |
| `BRIGHTNESS` | 255 (the global FastLED brightness; time-of-day dimming is applied through `stripBrightness`) |
| `MAX_NUM_PIXELS` | 1024 (about 330 ft of strip; going beyond that needs a memory and timing review) |
| Gateway / DNS | 192.168.0.1 |
| `STATIC_BROADCAST_CLIENTS` | 10, 11, 12 |
| `NTP_SERVER_IP` | 192.168.0.58 (when `LOCAL_NTP_SERVER` is defined) |
| `MDNS_CACHING_TIMEOUT_MS` | 60 min |
| `IGNORE_WEB_EFFECT_CHANGES` | 0 by default |

# Adding a board

1. Add a `#if BOARD_ID == 4` block to `config.h` with every per-board macro.
2. Add the board to `$boardMap` in `scripts/boards.ps1` and to the `ValidateSet` of `build.ps1`, `update.ps1`, `ota_upgrade.ps1`, `status.ps1`, `files.ps1` and `upload_audio_seed.ps1`.
3. Add its last IP octet to `STATIC_BROADCAST_CLIENTS` if it should take part in [broadcast](/network/multi-board-broadcast.md).

[^config]: include/config.h
[^boards]: scripts/boards.ps1
