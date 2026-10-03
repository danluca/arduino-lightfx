---
type: Process
title: OTA firmware upgrade
description: The firmware binary is POSTed to /fw with a token and SHA-256, stored as /fw.bin, and verified. CORE0 is then notified, the FX task shows an upgrade pattern, and PicoOTA flashes the image on reboot.
tags: [ota, firmware, upgrade, picoota]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: web
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/web_server.cpp
    title: src/web_server.cpp (handleFWImageUpload)
  - id: ota
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/ota_upgrade.cpp
    title: src/ota_upgrade.cpp
  - id: otaps
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/ota_upgrade.ps1
    title: ota_upgrade.ps1
---

# Client side

```powershell
./ota_upgrade.ps1 -board FX01                 # builds rp2040-rel for FX01 and uploads it
./ota_upgrade.ps1 -board Dev -fwPath my.bin   # uploads an existing binary
```

The script builds in **release** mode (debug builds need USB), computes the SHA-256, and sends `POST http://<board>/fw` with the headers `X-Token` and `X-Check: <sha256>` (`application/octet-stream`).[^otaps]

# Board side

1. **Upload** (`handleFWImageUpload`, a raw streaming handler on CORE0):
   * `RAW_START`: checks `X-Token` and deletes any old `/fw.bin`.
   * `RAW_WRITE`: appends each chunk to `/fw.bin` through the FS task.
   * `RAW_END`: computes the SHA-256 of the file. On a match it responds `200 {"status":"OK"}` and notifies CORE0 with `OTA_UPGRADE_NOTIFY` (0xDC). On a mismatch it responds `406`. A bad token gets `401`.
   * `RAW_ABORTED`: responds `400` and deletes the partial file.[^web]
2. **Trigger:** the next CORE0 `loop()` calls `handle_fw_upgrade()`, which sees the notification and then:
   * notifies the **FX** task, which stops rendering effects and shows a fixed red, green, gray and violet "upgrade" pattern while still feeding the watchdog;
   * after 3 s, disables the watchdog and calls `Scheduler.suspendAllTasks()`;
   * `picoOTA.begin(); addFile("/fw.bin"); commit();` then `LittleFS.end()`;
   * writes reset marker `0xA11CE502` and calls `rp2040.reboot()`.[^ota]
3. **Flash:** the arduino-pico OTA bootloader applies `/fw.bin` on the next boot.

# Notes

* Flash budget: 4 MB maximum firmware (`board_upload.maximum_size`) and a 768 KB filesystem. The staged image must fit in LittleFS, so keep firmware well under 768 KB, or raise `board_build.filesystem_size`.
* The generic `/upload` endpoint refuses to write to `/fw.bin`.
* `update.ps1` uses a different path: Arduino-CLI network OTA (`--protocol network`), falling back to USB. That OTA path has a bug. See [update.ps1 OTA env bug](/issues/update-ps1-ota-env.md).
* Watch out: the upload token is a static shared secret (`FW_AUTH_TOKEN` in the git-ignored `include/secrets.h`; scripts read it via `LIGHTFX_AUTH_TOKEN` or that file). See [hard-coded upload token](/issues/hardcoded-upload-token.md).

[^web]: src/web_server.cpp (handleFWImageUpload)
[^ota]: src/ota_upgrade.cpp
[^otaps]: ota_upgrade.ps1
