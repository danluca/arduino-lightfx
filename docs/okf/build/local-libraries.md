---
type: Reference
title: Local libraries
description: The six libraries vendored under lib/, where each comes from, and why the project carries its own copy.
tags: [libraries, dependencies, lib]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: lib
    resource: https://github.com/danluca/arduino-lightfx/tree/6d11a87/lib
    title: lib/*/library.json
---

| Library | Version | Origin | Role in this project |
|---|---|---|---|
| `FilesystemTask` | 1.0.0 | Dan Luca | Serializes all LittleFS access through one FS task. Provides `SyncFsImpl` with `readFile`, `writeFile`, `writeFileAsync`, `appendFile`, `list`, `sha256`, `mkdir`, `remove`. See [filesystem layout](/persistence/filesystem-layout.md) |
| `PicoLog` | 1.0.0 | Dan Luca | `log_debug/info/warn/error/write` macros feeding a ring buffer that the SRL task streams to serial. The macros compile to nothing unless `LOGGING_ENABLED=1` |
| `SchedulerExt` | 0.4.4.1 | Based on the mbed `Scheduler` | `Scheduler.startTask(&TaskDef)` with separate setup and loop functions, core affinity and priority (`vTaskDelay(1)` after each loop call); also `suspendAllTasks()` |
| `TimeLib` | 1.0.0 | Combines Paul Stoffregen's Time, Jack Christensen's Timezone and an NTP client | `TimeService`, `Timezone`, `TimeFormat`, `CoreTimeCalc`, `NTPClient`, `now()` |
| `RestWebServer` | 2.0.0 | arduino-pico `WebServer` (Ivan Grokhotkov, Earle Philhower), ported | One-client HTTP server over the framework `WiFi` classes. Supports raw upload handlers with a transfer heartbeat, and static serving from flash maps |
| `Utils` | 1.0.0 | Dan Luca | `FixedQueue` (bounded FIFO used for history, time syncs, reboots and recipients), `CircularBuffer`, `StringUtils` |

The RP2040 line carries three more: `RP2040WiFiNina` (NINA-W102 driver), `LightMDNS` (replaced here by the framework's `LEAmDNS`) and `FHT`. None of them are needed on the Plasma 2350 W.

Some `library.json` metadata was not updated for this board: `RestWebServer` still describes itself as a WiFiNINA derivative that avoids lwIP, and `TimeLib` says it is "not yet updated for RP2350". Both work on the RP2350.

These libraries are maintained alongside the firmware ("Libraries robustness updates", "fixed FsRequestPool hang"), so fixes may need to be ported to the RP2040 line as well.[^lib]

[^lib]: lib/*/library.json
