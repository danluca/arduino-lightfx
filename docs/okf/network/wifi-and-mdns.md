---
type: Architecture
title: Wi-Fi and mDNS
description: Wi-Fi through the on-board CYW43439 and lwIP with DHCP addressing, a health check every 7 minutes (gateway ping plus RSSI), a full reconnect procedure, and mDNS advertising and discovery of the _lucasfx service.
tags: [wifi, cyw43, lwip, mdns, network]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: net
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/net_setup.cpp
    title: src/net_setup.cpp
  - id: config
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/config.h
    title: include/config.h
  - id: lwip
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/lwipopts.h
    title: include/lwipopts.h
  - id: board
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/boards/pimoroni_plasma2350w.json
    title: boards/pimoroni_plasma2350w.json
---

# Stack

* **Radio:** the Plasma 2350 W's on-board Infineon CYW43439 (2.4 GHz 802.11b/g/n), enabled by `-DPICO_CYW43_SUPPORTED=1 -DCYW43_PIN_WL_DYNAMIC=1` in the board file.[^board]
* **Driver and IP stack:** the arduino-pico `WiFi` library over lwIP. The project overrides lwIP options in `include/lwipopts.h` (32 KB `MEM_SIZE`, more PCBs and pbufs than stock, mDNS responder, IPv4 and IPv6).[^lwip] There is no local Wi-Fi driver library.
* **Credentials:** `include/secrets.h` (git-ignored; template `include/secrets.h.example`) must define `WF_SSID` and `WF_PSW`, plus the upload token `FW_AUTH_TOKEN`.
* **Addressing:** **DHCP**. `WiFi.config({IP_ADDR})` is commented out; the router reserves a fixed address per board MAC (Dev 192.168.0.75, Tree 192.168.0.182). After connecting, `SysInfo` takes the IP and gateway from the Wi-Fi stack; `IP_ADDR`/`IP_GW` in `config.h` are only defaults until then. Hostname: `lightfx-<DEVICE_NAME>`, for example `lightfx-FXPine`.[^net]
* **Power:** `WiFi.noLowPowerMode()` and station mode (`WIFI_STA`).

# Connect

`wifi_connect()` sets the hostname, then loops `WiFi.begin` with a 30-second wait per attempt (`setTimeout(7500)`), checking in with the health monitor every 500 ms and calling `disconnect()` between attempts, until it connects. It then sets `SysStatus::Wifi`, records SSID, IP, gateway, DNS and MAC in `SysInfo`, pings the gateway, and starts mDNS.[^net]

`checkFirmwareVersion()` is a leftover from the NINA line: on the CYW43 both the reported firmware version and `WIFI_FIRMWARE_LATEST_VERSION` are the Pico SDK version string, so the check never warns.

# Health check: `wifi_ensure` (every 7 min, CORE0)

`wifi_check()` treats the link as unusable when the status is not `WL_CONNECTED`, when 4 gateway pings (128-byte, 100 ms apart) all fail, or when **RSSI < −75 dBm** (2 bars or fewer). In that case `wifi_reconnect()` runs:

1. Clear `Wifi`. Stop the web server. End the time service and delete `ntpUDP`. Remove the mDNS service queries and services, then `MDNS.close()`.
2. `WiFi.disconnect()` + `WiFi.end()`.
3. Wait 2 s, then `wifi_connect()`, `MDNS.announce()`, and `web::server_setup()` (the handlers are only registered once).

Afterwards, if Wi-Fi is up, it posts a time setup check, which retries NTP if it is still missing.

# mDNS (`MDNS_ENABLED=1`)

* The framework's `LEAmDNS` responder (from the ESP8266 lineage), not a local library.
* Host: `lightfx-<device>.local` in lowercase. Service: `<host>._lucasfx._tcp` on port 80, with TXT records `info` ("Pimoroni Plasma 2350W Lucas LightFX") and `model` ("Plasma 2350W").
* **Discovery:** a service query for `_lucasfx._tcp` records every `lightfx-*` host in `discoveredBoards` (mutex-protected, entries expire after `MDNS_CACHING_TIMEOUT_MS` = 60 min). `scanClients` adds them as [broadcast](/network/multi-board-broadcast.md) recipients.
* `MDNS.update()` runs only while the web server is idle.
* The `platformio.ini` comment notes mDNS works well with Linux hosts but is flaky with Windows.

Signal bars (`barSignalLevel`) map RSSI linearly from −100 dBm (0 bars) to −55 dBm (4 bars).

[^net]: src/net_setup.cpp
[^config]: include/config.h
[^lwip]: include/lwipopts.h
[^board]: boards/pimoroni_plasma2350w.json
