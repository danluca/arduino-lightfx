---
type: Architecture
title: Wi-Fi and mDNS
description: Wi-Fi through the NINA-W102 module with static IPs per board, a health check every 7 minutes (gateway ping plus RSSI), a full reconnect procedure, and mDNS advertising of the _lucasfx service.
tags: [wifi, nina, mdns, network]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: net
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/net_setup.cpp
    title: src/net_setup.cpp
  - id: config
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/include/config.h
    title: include/config.h
---

# Stack

* **Driver:** a local copy of WiFiNINA (`lib/RP2040WiFiNina`, synced with 2.0.1). It is used instead of lwIP/CYW43; `build_unflags` removes the `CYW43_LWIP`/`LWIP_*` defines to save memory.
* **Credentials:** `include/secrets.h` (git-ignored; template `include/secrets.h.example`) must define `WF_SSID` and `WF_PSW`, plus the upload token `FW_AUTH_TOKEN`.
* **Addressing:** static IP per board (`IP_ADDR`). The gateway and DNS are `192.168.0.1` with mask `/24`. Hostname: `lightfx-<DEVICE_NAME>`.[^config]
* **Power:** `WiFi.lowPowerMode()`. The web server is not the board's main job.

# Connect

`wifi_connect()` runs `WiFi.config(...)`, then loops `WiFi.begin` with a 30-second wait per attempt (`setTimeout(7500)`), calling `disconnect()` between attempts until it connects. It then sets `SysStatus::Wifi`, pings the gateway, logs the SSID, IP, MAC and RSSI, and starts mDNS. `checkFirmwareVersion()` warns when the NINA firmware is older than `WIFI_FIRMWARE_LATEST_VERSION`. Update it with `scripts/nina_fw_update.ps1`.[^net]

# Health check: `wifi_ensure` (every 7 min, CORE0)

`wifi_check()` treats the link as unusable when the status is not `WL_CONNECTED` (checked twice, 100 ms apart), when 4 gateway pings all fail, or when **RSSI < −75 dBm** (2 bars or fewer). In that case it runs `wifi_reconnect()`:

1. Clear `Wifi`. Stop the web server. End the time service and delete `ntpUDP`. Stop and delete `mdns` and `mUdp`.
2. `WiFi.disconnect()` + **`WiFi.end()`**. Without `end()`, sockets after a reconnect come back closed (see arduino/nina-fw#63).
3. Wait 2 s, then `wifi_connect()`, then `web::server_setup()` (the handlers are only registered once).

Afterwards, if NTP is still missing, it schedules a time setup retry.

# mDNS (`MDNS_ENABLED=1`)

* `lib/LightMDNS` (RFC 6762, announce/answer only, built without exceptions).
* Host: `lightfx-<device>.local` in lowercase. Service: `<host>._lucasfx._tcp` on port 80, with TXT records `info`, `name`, `model`.
* `mdns->process()` runs only when the web server is idle.
* The `platformio.ini` comment notes mDNS works well with Linux hosts but is flaky with Windows.

Signal bars (`barSignalLevel`) map RSSI linearly from −100 dBm (0 bars) to −55 dBm (4 bars).

[^net]: src/net_setup.cpp
[^config]: include/config.h
