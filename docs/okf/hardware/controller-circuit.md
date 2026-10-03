---
type: Hardware
title: Controller circuit and LED strip
description: How a 3.3 V Nano RP2040 Connect drives a 12 V WS2811 strip through a 7805 regulator and a 74HCT125 level shifter, plus the strip model and the alternative board.
tags: [hardware, circuit, ws2811, level-shifter]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: readme
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/README.md
    title: README.md (Hardware Integration)
    author: human:danluca
  - id: schem
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/docs/Controller_Schematic.pdf
    title: docs/Controller_Schematic.pdf
  - id: easyeda
    resource: https://pro.easyeda.com/editor#id=9c50130b250b4c23b522b4ac978d99bf
    title: EasyEDA project
---

# Circuit

| Part | Role |
|---|---|
| Arduino Nano RP2040 Connect | Controller (RP2040, NINA-W102 Wi-Fi, LSM6DSOX IMU, ECC608 crypto, MP34DT06JTR PDM mic, 16 MB AT25SF128A flash) |
| LM7805 | 12 V → 5 V supply for the board |
| 74HCT125 | 3.3 V → 5 V level shifter for the data line |
| 3-pin connector | 12 V in, ground, 5 V data out to the strip |
| Resistor divider on A0 | Measures the supply voltage (values per board in `config.h`) |

Design files in the repo: `docs/Controller_Schematic.pdf`, `docs/Controller_PCB.pdf` and `docs/Controller_PCB.gerber.zip`. The EasyEDA project is linked in `sources`.[^readme][^schem]

# Strip

* BTF-Lighting **WS28115M30LW65**: 12 V WS2811, 30 LEDs/m, IP65.
* One WS2811 "pixel" is a group of three LEDs.
* The longest run in the house is about 300 pixels (board FX01).
* Data pin: GPIO 25 / D2. FastLED drives it with RP2040 PIO (non-blocking).

# Alternative board

The README mentions that the **Pimoroni Plasma Stick 2040 W** (5 V) works with minimal changes. A separate RP2350 code line targets the Plasma 2350 W and Pico 2 W. See [documentation drift](/issues/documentation-drift.md).

[^readme]: README.md (Hardware Integration)
[^schem]: docs/Controller_Schematic.pdf
