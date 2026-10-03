---
type: Playbook
title: Build and deploy
description: How to build per board with the PowerShell scripts or plain PlatformIO, flash over USB or HTTP OTA, capture serial logs and query a running board.
tags: [build, deploy, platformio, powershell, ota]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: build
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/build.ps1
    title: build.ps1
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/scripts/util.ps1
    title: scripts/util.ps1
  - id: instr
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/.claude/instructions.md
    title: .claude/instructions.md
    author: human:danluca
---

# Prerequisites

* Python 3.12+ and the PlatformIO Core CLI (`pio`). PowerShell 7 (`pwsh`) for the scripts.
* `include/secrets.h` (git-ignored; copy `include/secrets.h.example`) defining `WF_SSID`, `WF_PSW` and the upload token `FW_AUTH_TOKEN`. The build fails without `FW_AUTH_TOKEN`.
* Optional: `arduino-cli` (for `update.ps1` network OTA) and a Python venv with `scripts/requirements.txt` (for audio seeds).
* Environment split: code is edited and compiled on Windows. The boards are attached over USB to an Ubuntu host, which also flashes them and captures serial output (AGENTS.md).

# Build

```powershell
./build.ps1                       # Dev board, rp2040-rel
./build.ps1 -board FX01 -log      # FX01 with LOGGING_ENABLED=1 (keeps USB serial)
./build.ps1 -dbg -clean           # rp2040-dbg after a clean
./build.ps1 -map                  # also write logs/firmware-<board>-<env>.map
```

`build.ps1` sets `PLATFORMIO_BUILD_FLAGS` (`-DBOARD_ID=…`, plus `-DLOGGING_ENABLED=1`, `-DIGNORE_WEB_EFFECT_CHANGES=1` or `-DPIO_FRAMEWORK_ARDUINO_NO_USB` as needed), records a transcript in `logs/build-<board>.log`, and runs `pio run -e <env>`. It also prints the commit of the arduino-pico framework it builds against.[^build][^util]

Plain PlatformIO: `pio run -e rp2040-rel`. In that case set `PLATFORMIO_BUILD_FLAGS` yourself to choose a board other than Dev. `scripts/pio_build.py` adds `GIT_COMMIT`, `GIT_COMMIT_SHORT`, `GIT_BRANCH` and `BUILD_TIME` automatically.

**Release builds without `-log` have no USB serial at all** (`PIO_FRAMEWORK_ARDUINO_NO_USB`). Use `-log` or `-dbg` when you need a console.

Agent rule from `.claude/instructions.md`: when you run builds inline, redirect the output to a file under `logs/` and create the folder if it does not exist.[^instr]

# Flash

| Method | Command | Notes |
|---|---|---|
| USB | `./update.ps1 -board Dev [-port COM5] [-log] [-dbg]` | Tries Arduino-CLI network OTA first, then falls back to `pio run -t upload` |
| HTTP OTA (preferred once deployed) | `./ota_upgrade.ps1 -board FX01` | Release build, then `POST /fw` with token and SHA-256. See [OTA firmware upgrade](/network/ota-firmware-upgrade.md) |
| Bootloader | Double-tap reset | `enableDoubleResetBootloader()` makes this enter UF2 mode |

`update.ps1` has a bug in its network OTA branch. See [update.ps1 OTA env bug](/issues/update-ps1-ota-env.md).

# Observe

* `./seriallog.ps1 [-port X]` runs `pio device monitor` and saves to `logs/log-<yyyyMMdd-HHmm>.log` (115200 baud).
* `scripts/status.ps1 -board X` and `scripts/files.ps1 -board X` fetch `/status.json` and `/files.json`.
* Browse to `http://<board-ip>/` for the UI and `/stats.html` for heap and task stats.

# Branching

Develop on `dev/12-fxe`. Release from `rel/nanorp2040connect`. The README asks for PRs against `dev/12-fxe` that describe any memory impact.

[^build]: build.ps1
[^util]: scripts/util.ps1
[^instr]: .claude/instructions.md
