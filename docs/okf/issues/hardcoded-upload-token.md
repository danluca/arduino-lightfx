---
type: Issue
title: Hard-coded upload token
description: The X-Token that authorizes POST /fw (firmware) and POST/PUT /upload is a static shared secret. It is now kept out of the repository (git-ignored include/secrets.h), but the current value is still in git history and has not been rotated.
tags: [security, risk, ota, upload]
severity: medium
issue_state: mitigated
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: web
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/web_server.cpp
    title: src/web_server.cpp (authToken)
  - id: ota
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/ota_upgrade.ps1
    title: ota_upgrade.ps1
---

# Situation

* `web_server.cpp` sets `authToken` from `FW_AUTH_TOKEN`, defined in the git-ignored `include/secrets.h` (template: `include/secrets.h.example`). The build fails with `#error` if it is missing. `handleFWImageUpload` and `handleFileUploadRaw` compare it with the `X-Token` request header.[^web]
* The scripts get the token from `Get-AuthToken` in `scripts/util.ps1` (`ota_upgrade.ps1`, `scripts/upload_audio_seed.ps1`) or the equivalent lookup in `scripts/upload_audio_seed.sh`: the `LIGHTFX_AUTH_TOKEN` environment variable if set, otherwise the `FW_AUTH_TOKEN` line in `include/secrets.h`.[^ota]
* Before `6d11a87` the value was hard-coded in the firmware and three scripts, so it remains in git history (on this branch and the RP2040 branches). It was deliberately **not rotated** yet.
* HTTP is plain text (port 80). Anyone on the LAN can sniff the token, and anyone who can read the repository has it.
* A SHA-256 (`X-Check`) protects integrity in transit, but it does not authenticate the uploader.
* Every other endpoint, including `PUT /fx`, has no authentication. That is a deliberate trusted-home-LAN design.

The token value is intentionally left out of this bundle.

# Exposure

Someone with LAN access and the token can flash arbitrary firmware or write files under `/ext/`. Until the token is rotated, anyone who can read the repository history has it.

# Options

1. ~~Move the token into the git-ignored `include/secrets.h` and have the scripts read it from an environment variable or a local untracked file.~~ Done.
2. Rotate the value: change `FW_AUTH_TOKEN` in `include/secrets.h` on every build host, then reflash every board (USB, or OTA with the *old* token via `LIGHTFX_AUTH_TOKEN`).
3. Optionally, sign firmware images and verify the signature on the board (this board has no secure element, so the key would live in flash).

[^web]: src/web_server.cpp (authToken)
[^ota]: ota_upgrade.ps1
