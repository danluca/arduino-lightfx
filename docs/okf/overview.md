---
type: Project Overview
title: RP2040 LightFX
description: Dual-core FreeRTOS firmware for the Arduino Nano RP2040 Connect that drives WS2811 LED strips with 50 registered effects, holiday palettes, a sleep schedule and a REST/web UI.
resource: https://github.com/danluca/arduino-lightfx
tags: [overview, rp2040, fastled, freertos, led]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: readme
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/README.md
    title: README.md
    author: human:danluca
    last_modified: 2026-10-03T00:00:00Z
  - id: main
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/Main.cpp
    title: src/Main.cpp
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/platformio.ini
    title: platformio.ini
---

# Summary

RP2040 LightFX is a personal, house-installation LED controller. It drives one WS2811 strip (12 V, BRG color order) from GPIO 25 (pin D2) of an **Arduino Nano RP2040 Connect**. It picks a new effect every 7 minutes using holiday-aware weights, dims at night, switches to a sleep light between 00:30 and 06:00, and serves a web UI and JSON API over the board's NINA-W102 Wi-Fi module.[^readme]

The README states its design priority directly: performance and features come before portability.[^readme]

# Key facts

| Aspect | Value |
|---|---|
| MCU | RP2040, dual Cortex-M0+ (133 MHz) |
| Framework | arduino-pico (Earle Philhower), from the owner's fork `danluca/arduino-pico#feat/stable`, with FreeRTOS SMP enabled[^pio] |
| LED library | FastLED pinned to 3.10.3[^pio] |
| FreeRTOS heap | 144 KB, heap_4 scheme (`configTOTAL_HEAP_SIZE`, `configFREERTOS_HEAP_SCHEME=4`)[^pio] |
| Flash | 16 MB physical, 4 MB configured, 768 KB LittleFS[^pio] |
| Effects | 50 registered across categories A–J (K is empty). See the [effect catalog](/effects/effect-catalog.md) |
| Firmware version | `1.2.0-<git short sha>` (`include/version.h`) |
| Default build env | `rp2040-rel` (debug: `rp2040-dbg`) |
| Branches | Development `dev/12-fxe`, release `rel/nanorp2040connect` |

# How the pieces fit

* **CORE0** handles setup, Wi-Fi, the web server, communications, alarms and the filesystem task. **CORE1** handles effect rendering (FX task), the microphone and diagnostics. See [task model](/architecture/task-model.md).[^main]
* Tasks share almost nothing directly. They talk through FreeRTOS queues fed by software timers and HTTP handlers. See [inter-task messaging](/architecture/inter-task-messaging.md).
* Effects are classes derived from `LedEffect`, registered in a global `EffectRegistry`. The registry creates one effect at a time and deletes it before creating the next, to limit heap fragmentation. See [effect registry](/effects/effect-registry.md).
* The FX task owns the hardware watchdog through `HealthMonitor`. A stalled render loop reboots the board. See [watchdog and health](/architecture/watchdog-and-health.md).

# Where to start

1. Read the [boot sequence](/architecture/boot-sequence.md) for the startup order of the two cores.
2. Read the [LED effect lifecycle](/effects/led-effect-lifecycle.md) and then [adding an effect](/effects/adding-an-effect.md).
3. Build and flash with the [build and deploy playbook](/build/build-and-deploy.md).
4. Check the [known issues](/issues/index.md) before relying on audio thresholds, the `-ignoreBroadcast` build, or `update.ps1` OTA.

# Related repositories

AGENTS.md describes an RP2350 (Pico 2 W / Plasma 2350 W) sibling of this project. This repository targets the RP2040 Nano Connect, and several commits port changes between the two ("ported enhancements from RP2350", "borrowed the 2350 seed files"). See [documentation drift](/issues/documentation-drift.md).

[^readme]: README.md
[^main]: src/Main.cpp
[^pio]: platformio.ini
