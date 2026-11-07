---
type: Reference
title: Build flags
description: Compile-time defines from platformio.ini, the build scripts and headers, and the behavior each one controls.
tags: [build, flags, platformio, config]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/platformio.ini
    title: platformio.ini
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/scripts/util.ps1
    title: scripts/util.ps1 (prepEnvironment)
---

# Environments

| Env | Purpose | Differences |
|---|---|---|
| `rp2040-rel` (default) | Normal builds | `pio_build.py` injects the git and build-time flags |
| `rp2040-dbg` | SWD debugging | `build_type = debug`, `-Og -ggdb -DFASTLED_ALLOW_INTERRUPTS=0`, `picoprobe` upload and debug, `LOGGING_ENABLED=1`, `DEBUG_RP2040_PORT=Serial`, fixed `GIT_COMMIT="DBG"` |

Common settings: `board = nanorp2040connect`, `board_build.core = earlephilhower`, `board_upload.maximum_size = 4194304`, `board_build.filesystem_size = 768k`, `monitor_speed = 115200`, `-Wall -fstack-protector-strong`.[^pio]

# Defines

| Define | Value | Effect |
|---|---|---|
| `PIO_FRAMEWORK_ARDUINO_ENABLE_FREERTOS` | set | FreeRTOS SMP under arduino-pico |
| `PIO_FRAMEWORK_ARDUINO_ENABLE_RTTI` | set | RTTI (needed by `std::variant`/`std::function` usage) |
| `configMAX_PRIORITIES` | 32 (both environments) | FreeRTOS priority range; app task priorities are derived from its midpoint |
| `configTIMER_QUEUE_LENGTH` | 24 | Room for the 14 software timers' commands |
| `configRECORD_STACK_HIGH_ADDRESS` | 1 | Stack stats |
| `configRUN_TIME_COUNTER_TYPE` | `uint64_t` | Per-task CPU % in `/tasks.json` |
| `configTOTAL_HEAP_SIZE` | 144 KB | See [memory model](/architecture/memory-model.md) |
| `configFREERTOS_HEAP_SCHEME` | 4 | heap_4 |
| `configCORE0_TASK_STACK_DEPTH` / `CORE1` | 2048 | CORE task stacks (needs the forked framework) |
| `MDNS_ENABLED` | 1 | LightMDNS plus client scanning |
| `LOG_BYPASS_BUFFER` | 1 | PicoLog bypass mode |
| `LOCAL_NTP_SERVER` | 1 | Use `NTP_SERVER_IP` from `config.h` |
| `PICO_CYW43_SUPPORTED` | 0 | No CYW43 (the Nano uses NINA). The CYW43/lwIP flags are also unflagged |
| `FASTLED_LEAN_AND_MEAN` | 1 | Smaller FastLED |
| `__TM_GMTOFF`, `__TM_ZONE` | `tm_offset`, `tm_zone` | Adds time zone fields to `struct tm` |
| `BOARD_ID` | 1/2/3 (script) | Selects the per-board config. See [boards](/hardware/boards-and-config.md) |
| `LOGGING_ENABLED` | 1 (script `-log`, dbg env) | Enables `log_*` macros (otherwise they compile to nothing) and the `diagInfo` timer |
| `IGNORE_WEB_EFFECT_CHANGES` | 1 (script `-ignoreBroadcast`) | Meant to ignore non-UI effect, auto and broadcast changes. **Does not compile today**. See [ignore-web-flag compile error](/issues/ignore-web-flag-compile-error.md) |
| `PIO_FRAMEWORK_ARDUINO_NO_USB` | script, when neither `-log` nor `-dbg` | Removes the USB stack and serial |
| `GIT_COMMIT`, `GIT_COMMIT_SHORT`, `GIT_BRANCH`, `BUILD_TIME` | `pio_build.py` | Embedded in `BUILD_VERSION = "1.2.0-<short>"` |
| `PLATFORMIO_LINKER_MAP` (env var) | script `-map` | Adds `-Wl,-Map,<path>` |

The comment in `platformio.ini` suggests adding `-w` to silence warnings temporarily, then removing it after library or major changes.[^pio][^util]

# Scheduler priorities

The application priority policy uses `base = configMAX_PRIORITIES / 2`: CORE0 and CORE1 start at `base`, FX uses `base + 1`, ALM and CORE0 after setup use `base - 1`, SRL uses `base - 2`, and FS remains one level above its caller (normally `base + 1`). At 32 levels these are 16, 17, 15, 14 and 17; at 12 levels they remain 6, 7, 5, 4 and 7 respectively. Compile-time checks keep these values valid and below the framework's highest two priorities.

The installed `framework-arduinopico` local definitions use `max/2` for CORE0/CORE1, `max-1` for the flash-coordination tasks, `max-2` for both LWIP and USB, and `configTIMER_TASK_PRIORITY=2` for Tmr Svc. At 32 levels that is 16, 31, 30, and 2. The package still defaults to 8 if not overridden; the announced upcoming revision is not present and its exact LWIP/USB priorities remain unverified.


[^pio]: platformio.ini
[^util]: scripts/util.ps1 (prepEnvironment)
