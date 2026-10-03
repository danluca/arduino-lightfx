---
type: Reference
title: Local libraries
description: The nine libraries vendored under lib/, where each comes from, and why the project carries its own copy.
tags: [libraries, dependencies, lib]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: lib
    resource: https://github.com/danluca/arduino-lightfx/tree/73c7243/lib
    title: lib/*/library.json
---

| Library | Version | Origin | Role in this project |
|---|---|---|---|
| `FilesystemTask` | 1.0.0 | Dan Luca | Serializes all LittleFS access through one FS task. Provides `SyncFsImpl` with `readFile`, `writeFile`, `appendFile`, `list`, `sha256`, `mkdir`, `remove`. See [filesystem layout](/persistence/filesystem-layout.md) |
| `PicoLog` | 1.0.0 | Dan Luca | `log_debug/info/warn/error/write` macros feeding a ring buffer that the SRL task streams to serial. The macros compile to nothing unless `LOGGING_ENABLED=1` |
| `SchedulerExt` | 0.4.4.1 | Based on the mbed `Scheduler` | `Scheduler.startTask(&TaskDef)` with separate setup and loop functions, core affinity and priority; also `suspendAllTasks()` |
| `TimeLib` | 1.0.0 | Combines Paul Stoffregen's Time, Jack Christensen's Timezone and an NTP client | `TimeService`, `Timezone`, `TimeFormat`, `CoreTimeCalc`, `now()` |
| `RestWebServer` | 2.0.0 | arduino-pico `WebServer` (Ivan Grokhotkov, Earle Philhower), ported | One-client HTTP server on WiFiNINA without lwIP. Supports raw upload handlers and static serving from flash maps |
| `RP2040WiFiNina` | 2.0.1-1 | Arduino WiFiNINA | NINA-W102 driver, kept in sync with upstream 2.0.1 |
| `LightMDNS` | 1.0.5 | Matthew Gream | RFC 6762 mDNS and DNS-SD (announce and answer), built without exceptions |
| `Utils` | 1.0.0 | Dan Luca | `FixedQueue` (bounded FIFO used for history, time syncs, reboots and recipients), `CircularBuffer`, `StringUtils` |
| `FHT` | 4.0.0 | OpenMusicLabs ArduinoFHT | Fast Hartley Transform. `include/fxI.h` defines `FHT_N 32`, but no current source file calls FHT functions. It looks like a leftover from an earlier live-audio VU meter |

Several of these were derived from upstream code to avoid pulling in lwIP or exceptions, or to make access thread-safe on FreeRTOS SMP. Recent commits ("Libraries robustness updates", "updated circular and fixed queue") show they are maintained alongside the firmware, so fixes may need to be ported to the RP2350 sibling project as well.[^lib]

[^lib]: lib/*/library.json
