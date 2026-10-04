---
type: Reference
title: Build flags
description: Compile-time defines from platformio.ini, the board file, the build scripts and headers, and the behavior each one controls.
tags: [build, flags, platformio, config]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/platformio.ini
    title: platformio.ini
  - id: board
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/boards/pimoroni_plasma2350w.json
    title: boards/pimoroni_plasma2350w.json
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/scripts/util.ps1
    title: scripts/util.ps1 (prepEnvironment)
---

# Environments

| Env | Purpose | Differences |
|---|---|---|
| `rp2350-rel` (default) | Normal builds | `-Wall`; `pio_build.py` injects the git and build-time flags |
| `rp2350-dbg` | SWD debugging | `build_type = debug`, `-Og -ggdb -DFASTLED_ALLOW_INTERRUPTS=0`, `-w`, `picoprobe` upload and debug at 5000 kHz, `LOGGING_ENABLED=1`, `LOG_BYPASS_BUFFER=1`, `DEBUG_RP2040_PORT=Serial`, `DEBUG_RP2040_CORE`, fixed `GIT_COMMIT="DBG"` |

Common settings: `platform = maxgerhardt/platform-raspberrypi`, `board = pimoroni_plasma2350w` (from `boards/`), `board_build.core = earlephilhower`, `board_build.filesystem_size = 1024k`, `monitor_speed = 115200`, `-fstack-protector-strong`. Library dependencies: `FastLED @ 3.10.3`, `ArduinoJson ^7.0.0`, `StreamUtils ^1.9.1`.[^pio]

# Defines

| Define | Value | Effect |
|---|---|---|
| `PIO_FRAMEWORK_ARDUINO_ENABLE_FREERTOS` | set | FreeRTOS SMP under arduino-pico |
| `PIO_FRAMEWORK_ARDUINO_ENABLE_RTTI` | set | RTTI (needed by `std::variant`/`std::function` usage) |
| `configMAX_PRIORITIES` | 12 | Task priority range |
| `configTIMER_QUEUE_LENGTH` | 24 | Room for the 14 software timers' commands |
| `configRECORD_STACK_HIGH_ADDRESS` | 1 | Stack stats |
| `configRUN_TIME_COUNTER_TYPE` | `uint64_t` | Per-task CPU % in `/tasks.json` |
| `configTOTAL_HEAP_SIZE` | 192 KB | See [memory model](/architecture/memory-model.md) |
| `configFREERTOS_HEAP_SCHEME` | 4 | heap_4 |
| `configCORE0_TASK_STACK_DEPTH` / `CORE1` | 3072 (words) | CORE task stacks (needs the forked framework) |
| `MDNS_ENABLED` | 1 | `LEAmDNS` responder plus board discovery |
| `LOCAL_NTP_SERVER` | 1 | Use `NTP_SERVER_IP` from `config.h` |
| `LOG_BYPASS_BUFFER` | 1 (dbg only; commented out in rel) | PicoLog bypass mode |
| `PICO_CYW43_SUPPORTED`, `CYW43_PIN_WL_DYNAMIC` | 1 (board file) | Enables the CYW43439 Wi-Fi driver |
| `ARDUINO_PIMORONI_PLASMA2350`, `ARDUINO_PIMORONI_PLASMA2350W`, `ARDUINO_ARCH_RP2040` | set (board file) | Board and architecture selection. `ARDUINO_ARCH_RP2040` is the arduino-pico architecture name for both chips |
| `__TM_GMTOFF`, `__TM_ZONE` | `tm_offset`, `tm_zone` | Adds time zone fields to `struct tm` |
| `BOARD_ID` | 1/2 (script); default 2 in `config.h` | Selects the per-board config. See [boards](/hardware/boards-and-config.md) |
| `LOGGING_ENABLED` | 1 (script `-log`, dbg env) | Enables `log_*` macros (otherwise they compile to nothing) and the `diagInfo` timer |
| `IGNORE_WEB_EFFECT_CHANGES` | 1 (script `-ignoreBroadcast`) | Ignores non-UI effect, auto and broadcast changes |
| `PIO_FRAMEWORK_ARDUINO_NO_USB` | script, when neither `-log` nor `-dbg` | Removes the USB stack and serial |
| `GIT_COMMIT`, `GIT_COMMIT_SHORT`, `GIT_BRANCH`, `BUILD_TIME` | `pio_build.py` | Embedded in `BUILD_VERSION = "1.2.0-<short>"` |
| `PLATFORMIO_LINKER_MAP` (env var) | script `-map` | Adds `-Wl,-Map,<path>` |
| `DIAG_CORE_HEARTBEATS`, `DIAG_WDT_PING_CORE0` | 1 (`constants.hpp`) | Temporary toggles from the watchdog investigation. `DIAG_CORE_HEARTBEATS` adds the CORE0/CORE1 heartbeat registers to the system info log; `DIAG_WDT_PING_CORE0` is not read anywhere |

There is no `build_unflags` and no `FASTLED_LEAN_AND_MEAN` on this board; those belong to the RP2040 line, which strips lwIP and CYW43. The comment in `platformio.ini` suggests adding `-w` to silence warnings temporarily, then removing it after library or major changes.[^pio][^util][^board]

[^pio]: platformio.ini
[^board]: boards/pimoroni_plasma2350w.json
[^util]: scripts/util.ps1 (prepEnvironment)
