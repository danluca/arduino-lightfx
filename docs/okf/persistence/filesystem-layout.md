---
type: Data Model
title: Filesystem layout
description: The LittleFS files (state, system info, generated configs, calibration, staged firmware and uploaded assets), their keys and writers. All access goes through the FilesystemTask wrapper.
tags: [filesystem, littlefs, persistence, json, state]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: const
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/include/constants.hpp
    title: include/constants.hpp (file names, JSON keys)
  - id: efx
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/efx_setup.cpp
    title: src/efx_setup.cpp (readFxState, saveFxState)
  - id: sysinfo
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/sysinfo.cpp
    title: src/sysinfo.cpp (readSysInfo, saveSysInfo)
  - id: fslib
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/lib/FilesystemTask/README.md
    title: lib/FilesystemTask/README.md
---

# Access rule

LittleFS is **not thread-safe**. Every read and write goes through `SyncFsImpl` (`lib/FilesystemTask`), which funnels calls to a dedicated FS task one at a time. Do not call `LittleFS` directly from application tasks. The one exception is `LittleFS.end()` right before the OTA reboot.[^fslib]

Partition size: 768 KB (`board_build.filesystem_size`).

# Files

| Path | Written by (task, when) | Read when | Content |
|---|---|---|---|
| `/state.json` | FX: `saveFxState()` after every 7-min random switch, on `SAVE_STATE`, and after a broadcast toggle | FX setup | `autoFxRoll`, `randomSeed`, `curFx` (registry index), `stripBrightness`, `audioThreshold`, `colorTheme`, `autoColorAdjust`, `sleepEnabled`, `broadcast` |
| `/sys.json` | ALM: `saveSysInfo()` every 90 s | CORE0 boot (`readSysInfo`) | Build version, time and branch, board and device names, board UID, secure element serial, MAC, NINA firmware version, IP, gateway, heap and stack figures, status, `wdReboots[]` |
| `/status/fxconfig.json` | FX: once in `fx_setup()` | `saveSysInfo` merge | `holidayList[]` and `fx[] {description, name, registryIndex}` |
| `/status/sysconfig.json` | ALM: `saveSysInfo()` (fxconfig merged with sys info) | Served as **`GET /config.json`** for the web UI | fxconfig plus `arduinoPicoVersion`, `freeRTOSVersion`, CPU details, versions and the like |
| `/status/calibration.json` | CORE1: after a successful CPU temperature calibration | Diag setup | Calibration parameters and reference measurements |
| `/fw.bin` | CORE0: `POST /fw` | PicoOTA on reboot | The staged firmware image |
| `/ext/**` | CORE0: `POST/PUT /upload` | Effects (for example `/ext/fx/fxi4_seed1-4.txt`) | User assets |

# Behaviors worth knowing

* Watchdog reboot timestamps in `/sys.json` are restored **only if the build version is unchanged**. A new firmware build starts with a clean history.[^sysinfo]
* `curFx` is a **registry index**. Reordering effect registrations remaps saved state. See [adding an effect](/effects/adding-an-effect.md).
* A saved `colorTheme` other than `None` turns holiday auto mode **off** on restore. A saved `None` keeps it on.
* `www/*.json` is git-ignored. Any JSON pulled from a board for local testing does not belong in the repo.

[^const]: include/constants.hpp (file names, JSON keys)
[^efx]: src/efx_setup.cpp (readFxState, saveFxState)
[^sysinfo]: src/sysinfo.cpp (readSysInfo, saveSysInfo)
[^fslib]: lib/FilesystemTask/README.md
