---
type: Playbook
title: Hardware debugging
description: Step-through SWD debugging of the Plasma 2350 W with a Raspberry Pi Debug Probe, and which parts of the RP2040-era guide in docs/debugging.md still apply.
tags: [debugging, swd, openocd, picoprobe]
status: draft
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: dbg
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/docs/debugging.md
    title: docs/debugging.md
    author: human:danluca
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/platformio.ini
    title: platformio.ini (rp2350-dbg)
  - id: board
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/boards/pimoroni_plasma2350w.json
    title: boards/pimoroni_plasma2350w.json (debug section)
---

This concept is marked `draft`. `docs/debugging.md` was written for the Nano RP2040 Connect and warns that it "might be outdated"; no RP2350-specific debugging guide exists in the repository, and nothing here was confirmed on a board.[^dbg]

# What the project configures

* `rp2350-dbg` env: `debug_tool = picoprobe`, `upload_protocol = picoprobe`, `debug_speed = 5000`, `build_type = debug`, `debug_build_flags = -Og -ggdb -DFASTLED_ALLOW_INTERRUPTS=0`.[^pio]
* Board file debug section: OpenOCD target `rp2350.cfg`, SVD `rp2350.svd`, J-Link device `RP2350_M33_0`.[^board]
* `debug_build_flags` sets `FASTLED_ALLOW_INTERRUPTS=0` as a workaround for FastLED issue #1481. With the pinned FastLED 3.10.3 this **breaks** the `rp2350-dbg` build: FastLED's `platforms/arm/compile_test.hpp` stops with `#error "RP2040 platforms should have FASTLED_ALLOW_INTERRUPTS set to 1"` (seen 2026-10-03). The workaround needs revisiting before the debug env can be used.

# What changes from the RP2040 guide

| `docs/debugging.md` step | On the Plasma 2350 W |
|---|---|
| Patch OpenOCD's `spi.c` for the Nano's Atmel AT25SF128A flash | Not needed for that chip. Use an OpenOCD build that supports the RP2350 (the Raspberry Pi fork or the one bundled with recent arduino-pico toolchains) |
| Solder wires to the fragile SWD pads on the back of the Nano | Check the Plasma 2350 W for an SWD header or pads before soldering |
| Symlink a custom `tool-openocd-raspberrypi` package | Only if the bundled OpenOCD lacks RP2350 support. The commented-out `platform_packages` line in `platformio.ini` still points at the RP2040 build |
| PlatformIO cannot parse the RP2040 SVD | The board file names `rp2350.svd`; peripheral view support is unverified |

# Steps (summary)

1. Hardware: a Raspberry Pi Debug Probe and a JST-SH 3-pin SWD cable to the board's SWD connection. The debug UART can go to the header pins.
2. In VS Code or CLion, select `env:rp2350-dbg` and run **PIO Debug**. The `picoprobe` upload and debug tool runs at 5000 kHz.
3. Keep the debugger's watchdog behavior in mind: `watchdog_enable(8192, true)` pauses the watchdog while the core is halted, so breakpoints do not reboot the board.

[^dbg]: docs/debugging.md
[^pio]: platformio.ini (rp2350-dbg)
[^board]: boards/pimoroni_plasma2350w.json (debug section)
