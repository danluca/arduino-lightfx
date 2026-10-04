---
type: Configuration
title: Boards and configuration
description: Per-board compile-time configuration selected with BOARD_ID (1 Dev "Xmas2350", 2 Tree "FXPine"), covering chipsets, pixel counts, frame sizes, addresses, measured voltages and LED wiring.
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

`BOARD_ID` defaults to **2** (Tree) in `config.h`. The build scripts set it through `PLATFORMIO_BUILD_FLAGS=-DBOARD_ID=<id>` based on `-board`, whose default is `Dev`. A plain `pio run` without the scripts therefore builds for Tree.[^boards][^config]

| `-board` | `BOARD_ID` | `DEVICE_NAME` | Chipset / order | `NUM_PIXELS` | `FRAME_SIZE` | Installation |
|---|---:|---|---|---:|---:|---|
| Dev | 1 | `Xmas2350` | `WS2812B` / `BGR` | 66 | 22 | 10 m Pimoroni LED star wire on the Christmas tree: 6 strands, 22 px per pair of strands |
| Tree | 2 | `FXPine` | `WS2811` / `RGB` | 1000 | 75 | Blue pine tree lighting (900 px measured, plus a reserve) |

`PIXEL_BUFFER_SPACE = 4 × FRAME_SIZE` for both boards. Each board also defines its measured 3V3 rail (`V3_3`, `MV3_3`, both 3.317 V) and the A0 voltage divider resistors (`VCC_DIV_R4` 22 kΩ, `VCC_DIV_R5` 4.7 kΩ), which are used for supply voltage readings.[^config]

# Addresses

| `-board` | `IP_ADDR` in `config.h` | Address in `scripts/boards.ps1` | mDNS host |
|---|---|---|---|
| Dev | 192.168.0.75 | 192.168.0.75 | `lightfx-xmas2350.local` |
| Tree | 192.168.0.182 | 192.168.0.182 | `lightfx-fxpine.local` |

The boards use **DHCP** with a router reservation per MAC; `IP_ADDR` is not applied to the interface and only serves as the default until Wi-Fi reports the real address. The scripts talk to the `boards.ps1` address (and `scripts/upload_audio_seed.sh` to the Dev address), so keep `config.h`, `boards.ps1` and the router reservations in step.

# Shared settings

| Setting | Value |
|---|---|
| `LED_PIN` | `PIN_NEOPIXEL` = GPIO 15, the Plasma 2350 W's LED data output[^variant] |
| Status LED | RGB on GPIO 16/17/18 (`PIN_LED_R/G/B`), active-low |
| `BRIGHTNESS` | 255 (the global FastLED brightness; time-of-day dimming is applied through `stripBrightness`) |
| `MAX_NUM_PIXELS` | 1024 (going beyond that needs a memory and timing review). The Tree board is close to it |
| Gateway / DNS | 192.168.0.1 (`IP_GW`, `IP_DNS`) |
| `STATIC_BROADCAST_CLIENTS` | 10, 11, 12 (the RP2040 boards) |
| `NTP_SERVER_IP` | 192.168.0.58 (when `LOCAL_NTP_SERVER` is defined) |
| `MDNS_CACHING_TIMEOUT_MS` | 60 min |
| `IGNORE_WEB_EFFECT_CHANGES` | 0 by default |

# Adding a board

1. Add a `#if BOARD_ID == 3` block to `config.h` with every per-board macro (`COLOR_ORDER`, `CHIPSET`, `NUM_PIXELS`, `FRAME_SIZE`, `PIXEL_BUFFER_SPACE`, `IP_ADDR`, `V3_3`, `MV3_3`, `VCC_DIV_R4`, `VCC_DIV_R5`, `DEVICE_NAME`).
2. Add the board to `$boardMap` in `scripts/boards.ps1` and to the `ValidateSet` of `build.ps1`, `update.ps1`, `ota_upgrade.ps1`, `scripts/status.ps1`, `scripts/files.ps1` and `scripts/upload_audio_seed.ps1`.
3. Add its last IP octet to `STATIC_BROADCAST_CLIENTS` if it should take part in [broadcast](/network/multi-board-broadcast.md) even when mDNS does not find it.

[^config]: include/config.h
[^boards]: scripts/boards.ps1
[^variant]: arduino-pico variants/pimoroni_plasma2350w/pins_arduino.h
