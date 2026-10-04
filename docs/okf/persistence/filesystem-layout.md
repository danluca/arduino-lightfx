---
type: Data Model
title: Filesystem layout
description: The LittleFS files (state, system info, generated configs, calibration, staged firmware and uploaded assets), their keys and writers. All access goes through the FilesystemTask wrapper.
tags: [filesystem, littlefs, persistence, json, state]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: const
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/constants.hpp
    title: include/constants.hpp (file names, JSON keys)
  - id: efx
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/efx_setup.cpp
    title: src/efx_setup.cpp (readFxState, saveFxState)
  - id: sysinfo
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/sysinfo_persist.cpp
    title: src/sysinfo_persist.cpp (readSysInfo, saveSysInfo, health events)
  - id: fslib
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/lib/FilesystemTask/README.md
    title: lib/FilesystemTask/README.md
---

# Access rule

LittleFS is **not thread-safe**. Every read and write goes through `SyncFsImpl` (`lib/FilesystemTask`), which funnels calls to a dedicated FS task one at a time. Do not call `LittleFS` directly from application tasks. The one exception is `LittleFS.end()` right before the OTA reboot.[^fslib]

Partition size: 1 MB (`board_build.filesystem_size = 1024k`).

# Files

| Path | Written by (task, when) | Read when | Content |
|---|---|---|---|
| `/state.json` | FX: `saveFxState()`, at most once per `fx_run` iteration, after a 7-min random switch, a `SAVE_STATE` message or a broadcast toggle (`fxStateDirty`) | FX setup | `autoFxRoll`, `randomSeed`, `curFx` (registry index), `stripBrightness`, `colorTheme`, `autoColorAdjust`, `sleepEnabled`, `broadcast` |
| `/sys.json` | ALM: `saveSysInfo()` every 317 s | CORE0 boot (`readSysInfo`) | Build version, time and branch, board and device names, board UID, `secElemId` (always empty here; no secure element), MAC, Wi-Fi firmware version (the Pico SDK version string on CYW43), IP, gateway, heap and stack figures, status, `wdReboots[]` |
| `/status/fxconfig.json` | FX: once in `fx_setup()` | `saveSysInfo` merge | `holidayList[]` and `fx[] {description, name, registryIndex}` |
| `/status/sysconfig.json` | ALM: `saveSysInfo()` (fxconfig merged with sys info) | Served as **`GET /config.json`** for the web UI | fxconfig plus `arduinoPicoVersion`, `freeRTOSVersion`, CPU details, versions and the like |
| `/status/calibration.json` | CORE1: on first boot (default reference point) and after a recalibration | Diag setup | Calibration parameters and reference measurements. See [diagnostic sensors](/hardware/diagnostic-sensors.md) |
| `/status/health_event.json` | CORE0: `reboot` section at the end of setup; any task: `slowness` section when the health monitor warns (async write) | Served as **`GET /health.json`** | Last reboot reason, reset marker, FX stage, FS-blocked marker; last slowness or stall gaps and CPU load. See [watchdog and health](/architecture/watchdog-and-health.md) |
| `/fw.bin` | CORE0: `POST /fw` | PicoOTA on reboot | The staged firmware image |
| `/ext/**` | CORE0: `POST/PUT /upload` | Effects (for example `/ext/fx/fxi4_seed1-4.txt`) | User assets |

# Behaviors worth knowing

* Watchdog reboot timestamps in `/sys.json` are restored **only if the build version is unchanged**. A new firmware build starts with a clean history.[^sysinfo]
* `curFx` is a **registry index**. Reordering effect registrations remaps saved state. See [adding an effect](/effects/adding-an-effect.md).
* A saved `colorTheme` other than `None` turns holiday auto mode **off** on restore. A saved `None` keeps it on.
* `www/*.json` is git-ignored. Any JSON pulled from a board for local testing does not belong in the repo.

[^const]: include/constants.hpp (file names, JSON keys)
[^efx]: src/efx_setup.cpp (readFxState, saveFxState)
[^sysinfo]: src/sysinfo_persist.cpp (readSysInfo, saveSysInfo, health events)
[^fslib]: lib/FilesystemTask/README.md
