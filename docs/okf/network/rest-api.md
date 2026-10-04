---
type: API Reference
title: REST API
description: HTTP endpoints served on port 80 by RestWebServer (one client at a time). They cover the web UI, status, health, tasks and config JSON, PUT /fx control, the file listing, and token-protected firmware and file uploads.
resource: http://lightfx-fxpine.local/
tags: [api, http, rest, web]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: web
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/web_server.cpp
    title: src/web_server.cpp
  - id: pixeljs
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/www/pixel.js
    title: www/pixel.js
---

# Server

`lib/RestWebServer` is a port of the arduino-pico WebServer, here built on the framework `WiFi` (lwIP) classes. It serves **one client at a time** and runs on the CORE0 loop, interleaved with comms, so a slow request delays comms and the reverse. Server agent: `rp2040-luca/1.0.0` (unchanged from the RP2040 line). JSON responses carry `Cache-Control: no-cache, no-store` and a `Date` header labeled "CST".[^web]

# Endpoints

| Method | Path | Purpose |
|---|---|---|
| GET | `/`, `/index.html`, `/pixel.css`, `/pixel.js`, `/stats.html`, `/stats.css`, `/stats.js` | The web UI, served **from flash**. The files are `www/*` compiled into `include/*_html.h` etc. Cached for 30 days, `immutable` |
| GET | `/<file>` | Otherwise served from LittleFS `/status/` |
| GET | `/file/<path>` | Any LittleFS file from `/` |
| GET | `/config.json` | `/status/sysconfig.json`: the effect list, holiday list and system info. The UI uses it to fill its dropdowns |
| GET | `/status.json` | Live status (see below) |
| GET | `/health.json` | `/status/health_event.json`: last reboot reason and last slowness or stall event. See [watchdog and health](/architecture/watchdog-and-health.md) |
| GET | `/tasks.json` | Heap stats, per-task stats from the 10-second snapshot (state, priorities, stack high-water mark, core affinity, CPU %), board name, UID and firmware version |
| GET | `/files.json?path=/x` | LittleFS directory listing. Paths containing `..` are rejected |
| PUT | `/fx` | Change settings (see below) |
| POST | `/fw` | Firmware upload. See [OTA firmware upgrade](/network/ota-firmware-upgrade.md) |
| POST, PUT | `/upload` | Generic file upload, confined to `/ext/` |
| * | anything else | 404 `text/plain` |

# `PUT /fx` body (JSON, every key optional)

| Key | Type | Effect |
|---|---|---|
| `auto` | bool | `AUTO_FX`: turn effect auto-roll on or off |
| `effect` | uint16 | `MANUAL_FX`: switch to this registry index. An optional `source` string records the master board's name |
| `holiday` | string | `COLOR_THEME`: a theme name, or `"None"` for auto |
| `brightness` | uint8 | `STRIP_BRIGHTNESS`: N>0 locks the brightness, 0 unlocks it |
| `sleepEnabled` | bool | `SLEEP_ENABLED` |
| `resetTempCal` | bool | `RESET_CALIBRATION` to diag (deletes `/status/calibration.json`) |
| `broadcast` | bool | `ENABLE_BROADCAST`: make this board the master |

There is no `audioThreshold` key on this board. The response is `{"updates": {...echoed values...}, "status": <last enqueue ok>}`. With `IGNORE_WEB_EFFECT_CHANGES=1`, requests that do not carry `X-Source: ui` cannot change `auto`, `effect` or `broadcast`, and an ignored `effect` change adds `"talkToHand": true` to the response. The UI sends `X-Source: ui`. Board-to-board calls send `User-Agent: rp2040-lightfx-master/1.0.0`.[^pixeljs]

Changes are **queued**. The response comes back before the FX task applies them, so `status.json` may still show the old values for a moment.

# `GET /status.json` sections

`watchdogRebootsCount`, `cleanBoot`, `lastWatchdogReboot`, `watchdogReboots[]` (`time`, `reason`, `marker`, `fxStage`, `fsBlocked`). `wifi{IP,bars,rssi,ssid}`. `fx{auto, sleepEnabled, asleep, autoTheme, theme, index, name, broadcast, ignoreWebFx, pastEffects[], brightness, brightnessLocked}`. `master{active, activeClients[] | masterBoard, knownClients[]}`. `time{ntpSync, millis, sdate, stime, time, dst, zoneDST, offset, zone, zoneShort, holiday, syncSize, averageDrift, lastDrift, totalDrift, currentDrift, syncs[], alarms[]}`. `temp{cpu{current, current_adc, min, min_adc, max, max_adc}}`. `vcc{current, min, max}`. `overallStatus`. `mdnsEnabled`. `upTime`, `bootTime`. `cpuTempCal{...}`.[^web]

Compared with the RP2040 line there are no audio fields and no board or Wi-Fi chip temperatures, and the web UI has no audio chart.

# Command-line helpers

`scripts/status.ps1 -board Tree`, `scripts/files.ps1 -board Dev`. Board addresses come from `scripts/boards.ps1`.

# Security notes

* There is no authentication on `GET` or `PUT /fx`. The design assumes a trusted home LAN.
* Uploads are protected only by a shared static `X-Token` (`FW_AUTH_TOKEN` from the git-ignored `include/secrets.h`). See [hard-coded upload token](/issues/hardcoded-upload-token.md).

[^web]: src/web_server.cpp
[^pixeljs]: www/pixel.js
