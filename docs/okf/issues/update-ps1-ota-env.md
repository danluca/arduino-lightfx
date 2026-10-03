---
type: Issue
title: update.ps1 OTA env bug
description: In update.ps1's Arduino-CLI network OTA branch, $brdEnv is never set, so it uploads .pio/build//firmware.bin. The USB fallback works.
tags: [bug, scripts, ota, powershell]
severity: low
issue_state: fixed
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: update
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/update.ps1
    title: update.ps1 (updateFirmwareOTA)
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/scripts/util.ps1
    title: scripts/util.ps1
---

# Defect

`updateFirmwareOTA` builds and then runs:

```powershell
arduino-cli upload ... --protocol network --port "$ipAddress" -i .pio/build/$brdEnv/firmware.bin
```

`$brdEnv` is only assigned inside `Update-FirmwareSerial` in `scripts/util.ps1`, which is a different function that this branch never calls. So the path resolves to `.pio/build//firmware.bin` and the upload fails.[^update][^util]

This branch only runs when `arduino-cli` is installed **and** `arduino-cli board list` shows a network-OTA board. Otherwise the script falls back to USB, which works.

# Related

* The OTA password is hard-coded as `"password"` in the same script.
* The project's main OTA route is `ota_upgrade.ps1` (HTTP `POST /fw`), which is not affected. See [OTA firmware upgrade](/network/ota-firmware-upgrade.md).

# Fix

Set `$brdEnv = Get-BoardEnvName $dbg` before the `arduino-cli upload` line.

# Resolution

Fixed in `d5de0ba`: the upload path uses `.pio/build/$(Get-BoardEnvName $dbg)/firmware.bin`.

[^update]: update.ps1 (updateFirmwareOTA)
[^util]: scripts/util.ps1
