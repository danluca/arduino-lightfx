---
type: Process
title: OTA firmware upgrade
description: The firmware binary is POSTed to /fw with a token and SHA-256, stored as /fw.bin, and verified. The filesystem task commits the PicoOTA command before responding with success. An atomic flag requests the upgrade pattern and hardware reboot.
tags: [ota, firmware, upgrade, picoota]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: web
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/web_server.cpp
    title: src/web_server.cpp (handleFWImageUpload)
  - id: ota
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/ota_upgrade.cpp
    title: src/ota_upgrade.cpp
  - id: otaps
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/ota_upgrade.ps1
    title: ota_upgrade.ps1
  - id: hm
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/HealthMonitor.cpp
    title: src/HealthMonitor.cpp (OTA mode)
---

# Client side

```powershell
./ota_upgrade.ps1 -board Tree                 # builds rp2350-rel for Tree and uploads it
./ota_upgrade.ps1 -board Dev -fwPath my.bin   # uploads an existing binary
```

The script builds in **release** mode (debug builds need USB), computes the SHA-256, and sends `POST http://<board>/fw` with the headers `X-Token` and `X-Check: <sha256>` (`application/octet-stream`).[^otaps]

# Board side

1. **Upload** (`handleFWImageUpload` in `src/ota_upload.cpp`, running on CORE0):
   * `RAW_START`: checks `X-Token` and prepares `/fw.bin` only for an authenticated request when no upgrade is already staged.
   * `RAW_WRITE`: appends bounded chunks through the filesystem task and records short writes as failures.
   * `RAW_END`: verifies the body length and SHA-256, then asks the filesystem task to call PicoOTA `begin`, `addFile`, and `commit`. Only successful staging sets the atomic pending flag and returns `200`. Failures return `401` (authorization), `409` (already staged), `406` (integrity), `507` (image storage), or `500` (OTA staging). Failed uploads clean up only their own image.
   * `RAW_ABORTED`: deletes an image only if that request owns it. An unauthenticated request cannot delete a staged image.
2. **Trigger:** FX reads the atomic pending flag and displays the upgrade pattern while feeding the watchdog. CORE0's next `handle_fw_upgrade()` waits 2 seconds, writes the OTA reset marker, and requests a hardware watchdog reboot. Application tasks remain alive; OTA does not force-delete tasks, suspend CORE1, or access LittleFS outside its owning task.
3. **Flash:** the arduino-pico OTA bootloader applies `/fw.bin` on the next boot. HTTP 200 confirms staging, not that the new firmware has booted.

# Notes

* Flash budget: 4 MB maximum firmware (`upload.maximum_size` in the board file) and a **1 MB** filesystem (`board_build.filesystem_size = 1024k`). The staged image must fit in LittleFS next to the other files, so keep the firmware well under 1 MB, or raise `board_build.filesystem_size`.
* The generic `/upload` endpoint refuses to write to `/fw.bin`. Further firmware uploads are rejected after an OTA command is staged.
* Image format and compatibility validation remain analysis only; see [OTA image validation analysis](../../ota-validation-analysis.md).
* `update.ps1` uses a different path: Arduino-CLI network OTA (`--protocol network`), falling back to USB. That OTA path has a bug. See [update.ps1 OTA env bug](/issues/update-ps1-ota-env.md).
* Watch out: the upload token is a static shared secret (`FW_AUTH_TOKEN` in the git-ignored `include/secrets.h`; scripts read it via `LIGHTFX_AUTH_TOKEN` or that file). See [hard-coded upload token](/issues/hardcoded-upload-token.md).

[^web]: src/ota_upload.cpp (handleFWImageUpload)
[^ota]: src/ota_upgrade.cpp
[^otaps]: ota_upgrade.ps1
[^hm]: src/HealthMonitor.cpp (OTA mode)
