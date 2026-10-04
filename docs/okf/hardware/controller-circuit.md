---
type: Hardware
title: Controller board and LED strips
description: The Pimoroni Plasma 2350 W controller, the pins the firmware uses, the two strip types it drives, and which repository hardware files belong to the RP2040 controller instead.
tags: [hardware, plasma2350w, ws2811, ws2812b]
status: draft
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: board
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/boards/pimoroni_plasma2350w.json
    title: boards/pimoroni_plasma2350w.json
  - id: config
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/config.h
    title: include/config.h
  - id: shop
    resource: https://shop.pimoroni.com/products/plasma-2350-w
    title: Pimoroni Plasma 2350 W product page
  - id: variant
    resource: https://github.com/earlephilhower/arduino-pico/blob/master/variants/pimoroni_plasma2350w/pins_arduino.h
    title: arduino-pico variants/pimoroni_plasma2350w/pins_arduino.h
---

This concept is marked `draft`: the repository does not document the physical wiring of the RP2350 installations. Everything below comes from the board definition, the pin map and `config.h`, not from a schematic.

# Controller

| Item | Value |
|---|---|
| Board | Pimoroni Plasma 2350 W, an RP2350 LED-strip driver board with a CYW43439 Wi-Fi module[^board][^shop] |
| Board definition | `boards/pimoroni_plasma2350w.json` in this repository: `mcu: rp2350`, `cpu: cortex-m33`, 150 MHz, 512 KB RAM, 4 MB firmware maximum, USB VID/PID `0x2E8A`/`0x10A6`, default upload `picotool` |
| LED data | `PIN_NEOPIXEL` = GPIO 15[^variant] |
| Status LED | RGB on GPIO 16/17/18, active-low |
| Supply sense | A0 (GPIO 26) through a `VCC_DIV_R4`/`VCC_DIV_R5` (22 kΩ / 4.7 kΩ) divider, per `config.h`[^config] |
| Entropy pin | `adcRandom()` reads the floating A1 (GPIO 27). Nothing calls it at present |

There is no IMU, secure element or microphone on this board. See [diagnostic sensors](/hardware/diagnostic-sensors.md).

# Strips

| Board | Strip | Notes |
|---|---|---|
| Dev (`Xmas2350`) | Pimoroni 10 m addressable RGB LED star wire, WS2812B, BGR | 66 pixels arranged as 6 strands on a Christmas tree |
| Tree (`FXPine`) | WS2811, RGB (Amazon listing B0923TN5GV, per the `config.h` comment) | 1000 pixels configured, about 900 measured |

FastLED drives the data pin through the RP2350 PIO, so the CPU does not bit-bang the strip.

# Hardware files in the repository

`docs/Controller_Schematic.pdf`, `docs/Controller_PCB.pdf` and `docs/Controller_PCB.gerber.zip` describe the **RP2040** controller: a Nano RP2040 Connect with an LM7805 regulator, a 74HCT125 level shifter and a 12 V WS2811 strip. They do not apply to the Plasma 2350 W. Check whether the Tree installation has its own supply and divider before trusting the `vcc` reading.

[^board]: boards/pimoroni_plasma2350w.json
[^config]: include/config.h
[^shop]: Pimoroni Plasma 2350 W product page
[^variant]: arduino-pico variants/pimoroni_plasma2350w/pins_arduino.h
