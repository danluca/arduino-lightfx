<!-- Copilot / AI assistant instructions for contributors and coding agents -->
# rp2040-lightfx — AI assistant guidance

Short, actionable guidance to help an AI coding agent be immediately productive in this repository.

- Architecture: The firmware hosts a set of LED effects where each effect is a subclass implementing a shared API. Effects auto-register with a global registry at startup; the registry drives lifecycle transitions and effect selection. See [src/EffectRegistry.cpp](src/EffectRegistry.cpp) and the base API in [include/LedEffect.h](include/LedEffect.h).

- Core responsibilities and boundaries:
    - `src/efx_setup.cpp` and `src/LedEffect*.cpp` implement effect creation and setup.
    - `src/EffectRegistry.cpp` manages transitions and selection logic.
    - `src/web_server.cpp` and files under `www/` expose configuration and REST APIs.
    - `src/sysinfo.cpp` contains heap/stack logging used for diagnostics.
    - Filesystem access is single-threaded via a dedicated FS task (see `lib/FilesystemTask` and usages in `src/*`).

- Platform & build:
    - Project uses PlatformIO. Primary envs: `rp2040-rel` (release-ish) and `rp2040-dbg` (debug). Inspect `platformio.ini` for build flags and memory-related defines.
    - Common commands:
      ```bash
      pio run -e rp2040-rel            # build
      pio run -e rp2040-rel -t upload # upload firmware
      pio device monitor -e rp2040-rel # open serial monitor
      pio run -e rp2040-dbg            # build debug
      pio debug -e rp2040-dbg          # debug (requires debug probe)
      ```

- Important project-specific conventions:
    - FreeRTOS + dual-core usage: CORE0 runs networking / web server; CORE1 runs FX and audio tasks. See README and `platformio.ini` build flags that enable FreeRTOS.
    - Heap is deliberately constrained; code uses wrapper flags (`-Wl,--wrap,malloc`) to instrument allocs. When modifying memory allocations prefer explicit cleanup on effect teardown.
    - Effects often allocate large `std::vector` buffers at `setup()` time (example: seed buffers in `src/fxI.cpp`). Implement `cleanup()` on effects that hold vectors or dynamic buffers; the LedEffect state machine will call it on transition to Idle or before calling another setup to avoid long-lived heap growth.
    - Filesystem access goes through `FilesystemTask` (see `lib/FilesystemTask/src`) — do not perform file I/O directly from multiple threads/tasks.
    - JSON usage is via `ArduinoJson`; watch `JsonDocument` lifetimes to avoid retained allocations.

- Integration & external deps:
    - FastLED for LED control (see `include/` and `src/*` effects).
    - ArduinoJson, WiFiNINA (used with Arduino-Pico core), several helper libs listed in `platformio.ini` `lib_deps`.
    - Custom OpenOCD setup is documented in the repo `README.md` for debugging with RP2040 boards.

- Typical investigation entry points (examples):
    - Memory/heap growth: search for `Heap      ::` log lines and inspect `src/sysinfo.cpp` for `logTaskStats()`; check large allocations in `src/fxI.cpp`.
    - Effect lifecycle bugs: inspect `include/LedEffect.h` and `src/EffectRegistry.cpp` to see how `setup()`/`run()`/`cleanup`/`idle` states are handled and how cleanup is transitoned through.
    - Web/config: `src/web_server.cpp` + `www/` for REST endpoints and static UI.

- Useful heuristics for code edits:
    - Keep changes minimal and platform-aware: respect `platformio.ini` flags (heap scheme and wrapped allocators). Run builds in `rp2040-rel` first to validate.
    - When adding runtime logging, prefer existing `sysinfo::logTaskStats()` to keep log format consistent.
    - For large file reads, prefer streaming or freeing buffers after use; reference `src/fxI.cpp` for a seed-file example.

- Context for AI interactions:
    - include folders ./include, ./src, ./lib that contain core code.
    - Tests are not present; focus on static analysis and adherence to existing patterns.
    - Recent code changes may indicate active areas; prioritize understanding those files first.

------
name: rp2040-lightfx AI assistant instructions
description: Instructions for AI coding assistants working on the rp2040-lightfx firmware repository.
applyTo: **
---
When modifying or adding LED effects that allocate dynamic memory (e.g., `std::vector`, heap buffers), ensure that the effect class overrides the `cleanup()` method to free or clear those resources. This prevents memory leaks and keeps heap usage stable during effect transitions.