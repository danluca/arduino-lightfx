---
type: Playbook
title: Build and deploy
description: How to build per board with the PowerShell scripts or plain PlatformIO, flash over USB or HTTP OTA, capture serial logs and query a running board.
tags: [build, deploy, platformio, powershell, ota]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: build
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/build.ps1
    title: build.ps1
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/scripts/util.ps1
    title: scripts/util.ps1
  - id: update
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/update.ps1
    title: update.ps1
  - id: instr
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/.claude/instructions.md
    title: .claude/instructions.md
    author: human:danluca
---

# Prerequisites

* Python 3.12+ and the PlatformIO Core CLI (`pio`). PowerShell 7 (`pwsh`) for the scripts.
* `include/secrets.h` (git-ignored; copy `include/secrets.h.example`) defining `WF_SSID`, `WF_PSW` and the upload token `FW_AUTH_TOKEN`. The build fails with `#error "FW_AUTH_TOKEN is not defined"` without it; a `secrets.h` from before `6d11a87` needs the new line.
* Optional: `arduino-cli` (for the network OTA branch of `update.ps1`) and a Python venv with `scripts/requirements.txt` (for audio seeds).
* Environment split: code is edited and compiled on Windows. The boards are attached over USB to an Ubuntu host, which also flashes them and captures serial output (AGENTS.md).

# Build

```powershell
./build.ps1                       # Dev board, rp2350-rel
./build.ps1 -board Tree -log      # Tree with LOGGING_ENABLED=1 (keeps USB serial)
./build.ps1 -dbg -clean           # rp2350-dbg after a clean
./build.ps1 -map                  # also write logs/firmware-<board>-<env>.map
./build.ps1 -ignoreBroadcast      # IGNORE_WEB_EFFECT_CHANGES=1
```

`build.ps1` accepts `-board Dev` or `-board Tree`. It sets `PLATFORMIO_BUILD_FLAGS` (`-DBOARD_ID=…`, plus `-DLOGGING_ENABLED=1`, `-DIGNORE_WEB_EFFECT_CHANGES=1` or `-DPIO_FRAMEWORK_ARDUINO_NO_USB` as needed), records a transcript in `logs/build-<board>.log`, and runs `pio run -e <env>`. It also prints the commit of the arduino-pico framework it builds against.[^build][^util]

Plain PlatformIO: `pio run -e rp2350-rel`. Without the scripts, `BOARD_ID` falls back to **2 (Tree)**, the default in `config.h`. Set `PLATFORMIO_BUILD_FLAGS=-DBOARD_ID=1` for Dev. `scripts/pio_build.py` adds `GIT_COMMIT`, `GIT_COMMIT_SHORT`, `GIT_BRANCH` and `BUILD_TIME` automatically.

**Release builds without `-log` have no USB serial at all** (`PIO_FRAMEWORK_ARDUINO_NO_USB`). Use `-log` or `-dbg` when you need a console. Without USB, the 1200-baud touch that PlatformIO uses to reboot into the bootloader also stops working; see the flash table below.

Agent rule from `.claude/instructions.md`: when you run builds inline, redirect the output to a file under `logs/` and create the folder if it does not exist.[^instr]

# Flash

| Method | Command | Notes |
|---|---|---|
| USB | `./update.ps1 -board Dev [-port /dev/ttyACM0] [-log] [-dbg]` | Tries Arduino-CLI network OTA first (FQBN `rp2350:rp2350:pimoroni_plasma2350`), then falls back to `pio run -t upload`. PlatformIO uploads with **`picotool`** (the board file's default protocol), using the 1200-baud touch to enter the bootloader |
| HTTP OTA (preferred once deployed) | `./ota_upgrade.ps1 -board Tree` | Release build, then `POST /fw` with token and SHA-256. See [OTA firmware upgrade](/network/ota-firmware-upgrade.md) |
| Bootloader | Hold BOOT while pressing RESET (or plugging in USB) | Needed for a release build without USB serial, since the 1200-baud touch has nothing to talk to. The double-reset bootloader is disabled in `setup()` |
| SWD | `pio run -e rp2350-dbg -t upload` | `picoprobe` with a Raspberry Pi Debug Probe. See [hardware debugging](/build/hardware-debugging.md) |

The firmware does not run an ArduinoOTA responder, so the Arduino-CLI network branch of `update.ps1` normally finds no board and falls through to USB.[^update]

# Observe

* `./seriallog.ps1 [-port X]` runs `pio device monitor` and saves to `logs/log-<yyyyMMdd-HHmm>.log` (115200 baud).
* `scripts/status.ps1 -board X` and `scripts/files.ps1 -board X` fetch `/status.json` and `/files.json` from the address in `scripts/boards.ps1`.
* Browse to `http://<board-ip>/` or `http://lightfx-<device>.local/` for the UI, `/stats.html` for heap and task stats, and `/health.json` for the last reboot cause.

# Branching

Develop on `dev/plasma2350w`. The RP2040 code line develops on `dev/12-fxe` and releases from `rel/nanorp2040connect`. Describe any memory impact in the PR.

[^build]: build.ps1
[^util]: scripts/util.ps1
[^update]: update.ps1
[^instr]: .claude/instructions.md
