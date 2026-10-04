---
type: Project Overview
title: RP2350 LightFX
description: Dual-core FreeRTOS firmware for the Pimoroni Plasma 2350 W that drives WS2811/WS2812B LED strips with 50 registered effects, holiday palettes, a sleep schedule and a REST/web UI.
resource: https://github.com/danluca/arduino-lightfx/tree/dev/plasma2350w
tags: [overview, rp2350, plasma2350w, fastled, freertos, led]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: main
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/Main.cpp
    title: src/Main.cpp
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/platformio.ini
    title: platformio.ini
  - id: board
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/boards/pimoroni_plasma2350w.json
    title: boards/pimoroni_plasma2350w.json
  - id: config
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/config.h
    title: include/config.h
---

# Summary

RP2350 LightFX is a personal LED controller for holiday and tree lighting. It drives one addressable strip from the Plasma 2350 W's on-board LED data connector (`PIN_NEOPIXEL`, GPIO 15) of a **Pimoroni Plasma 2350 W**. It picks a new effect every 7 minutes using holiday-aware weights, dims at night, switches to a sleep light between 00:30 and 06:00 when enabled, and serves a web UI and JSON API over the board's on-board **CYW43439** Wi-Fi (lwIP stack).[^main][^board]

The code line is a port of the RP2040 (Arduino Nano RP2040 Connect) project that lives on other branches of the same repository. The two share the effects engine, scheduling, persistence and web UI, but differ in board support, networking stack and diagnostics. See [documentation drift](/issues/documentation-drift.md) for the places where RP2040-era text survives.

# Key facts

| Aspect | Value |
|---|---|
| Board | Pimoroni Plasma 2350 W (custom board file `boards/pimoroni_plasma2350w.json`)[^board] |
| MCU | RP2350, dual Cortex-M33 at 150 MHz, 520 KB SRAM (512 KB declared as `maximum_ram_size`)[^board] |
| Wi-Fi | Infineon CYW43439 on-board module, arduino-pico `WiFi` + lwIP (custom `include/lwipopts.h`) |
| Framework | arduino-pico (Earle Philhower), from the owner's fork `danluca/arduino-pico#feat/stable`, with FreeRTOS SMP enabled[^pio] |
| LED library | FastLED pinned to 3.10.3[^pio] |
| FreeRTOS heap | 192 KB, heap_4 scheme (`configTOTAL_HEAP_SIZE`, `configFREERTOS_HEAP_SCHEME=4`)[^pio] |
| Flash | 4 MB firmware maximum (`upload.maximum_size`), 1 MB LittleFS (`board_build.filesystem_size = 1024k`)[^pio][^board] |
| Effects | 50 registered across categories A–J (K is empty). See the [effect catalog](/effects/effect-catalog.md) |
| Boards | `Dev` (`Xmas2350`, 66 px WS2812B star wire) and `Tree` (`FXPine`, 1000 px WS2811). See [boards](/hardware/boards-and-config.md)[^config] |
| Firmware version | `1.2.0-<git short sha>` (`include/version.h`) |
| Default build env | `rp2350-rel` (debug: `rp2350-dbg`) |
| Branch | Development `dev/plasma2350w` |

# How the pieces fit

* **CORE0** handles setup, Wi-Fi, the web server, communications, alarms (ALM task) and the filesystem (FS task). **CORE1** handles effect rendering (FX task) and diagnostics. There is no microphone task on this board. See [task model](/architecture/task-model.md).[^main]
* Tasks share almost nothing directly. They talk through four FreeRTOS queues fed by software timers and HTTP handlers. See [inter-task messaging](/architecture/inter-task-messaging.md).
* Effects are classes derived from `LedEffect`, registered in a global `EffectRegistry`. The registry creates one effect at a time and deletes it before creating the next, to limit heap fragmentation. See [effect registry](/effects/effect-registry.md).
* The FX task owns the hardware watchdog through `HealthMonitor`. A stalled render loop, or a CORE0 starved for more than 15 s, reboots the board. See [watchdog and health](/architecture/watchdog-and-health.md).

# Where to start

1. Read the [boot sequence](/architecture/boot-sequence.md) for the startup order of the two cores.
2. Read the [LED effect lifecycle](/effects/led-effect-lifecycle.md) and then [adding an effect](/effects/adding-an-effect.md).
3. Build and flash with the [build and deploy playbook](/build/build-and-deploy.md).
4. Check the [known issues](/issues/index.md), in particular the [documentation drift](/issues/documentation-drift.md): README.md, AGENTS.md and `.claude/instructions.md` still describe the RP2040 board.

# Related code lines

The RP2040 code line (`dev/12-fxe`, released from `rel/nanorp2040connect`) targets the Arduino Nano RP2040 Connect with a NINA-W102 Wi-Fi module, an IMU, an ECC608 secure element and a PDM microphone. Fixes in shared areas (effects, registry, scheduling, `lib/`) are regularly ported between the two lines, so check both when fixing a bug there.

[^main]: src/Main.cpp
[^pio]: platformio.ini
[^board]: boards/pimoroni_plasma2350w.json
[^config]: include/config.h
