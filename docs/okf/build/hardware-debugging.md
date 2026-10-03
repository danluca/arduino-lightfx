---
type: Playbook
title: Hardware debugging
description: Step-through SWD debugging of the Nano RP2040 Connect with a Raspberry Pi Debug Probe. It needs soldered SWD wires and an OpenOCD build that knows the AT25SF128A flash chip.
tags: [debugging, swd, openocd, picoprobe]
status: draft
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: dbg
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/docs/debugging.md
    title: docs/debugging.md
    author: human:danluca
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/platformio.ini
    title: platformio.ini (rp2040-dbg)
---

The source document warns: "These instructions might be outdated. Use at your own risk." This concept is marked `draft` for that reason.[^dbg]

# Why the setup is non-standard

* Stock OpenOCD does not know the board's **Atmel AT25SF128A** 16 MB NOR flash.
* The SWD pads are on the back of the board and are **fragile**. Wires must be soldered on carefully.
* FastLED does not compile in debug mode unless `FASTLED_ALLOW_INTERRUPTS=0` is set (FastLED issue #1481). The `rp2040-dbg` env sets it in `debug_build_flags`.[^pio]
* PlatformIO cannot parse the RP2040 SVD file, so the peripheral view is empty. Breakpoints, stepping and variable inspection still work.

# Steps (summary)

1. Hardware: a Raspberry Pi Debug Probe, JST-SH 3-pin SWD cables, and wires soldered to the SWD pads. The debug UART goes to the header pins.
2. Download Earle Philhower's OpenOCD from pico-quick-toolchain into `~/Code/Tools/openocd-rp2040-earle`.
3. Clone `raspberrypi/openocd`, branch `rp2040-v0.12.0`. Add this line to `flash_devices` in `src/flash/nor/spi.c`:
   `FLASH_ID("atmel 25sf128a", 0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x0001891f, 0x100, 0x1000, 0x1000000),`
   Then build (`./bootstrap && ./configure && make`) and copy `src/openocd` into the tools `bin/` folder.
4. Replace the PlatformIO OpenOCD package: `rm -rf ~/.platformio/packages/tool-openocd-raspberrypi`, then `pio pkg install --tool "tool-openocd-raspberrypi=symlink:///home/<you>/Code/Tools/openocd-rp2040-earle"`. That command rewrites `platformio.ini` and strips its comments, so undo that in your editor.
5. In VS Code, select `env:rp2040-dbg` and run **PIO Debug**. The `picoprobe` upload and debug tool runs at 5000 kHz.[^dbg]

[^dbg]: docs/debugging.md
[^pio]: platformio.ini (rp2040-dbg)
