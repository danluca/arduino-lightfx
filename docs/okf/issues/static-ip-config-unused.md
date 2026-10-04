---
type: Issue
title: Static IP config unused
description: The boards get their address from DHCP, but SysInfo keeps IP_ADDR and IP_GW from config.h as the board's own address and gateway. Broadcast self-detection and the gateway ping use those constants, and the Dev board's configured, scripted and actual addresses can all differ.
tags: [risk, network, config, broadcast]
severity: low
issue_state: fixed
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: net
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/net_setup.cpp
    title: src/net_setup.cpp (wifi_connect)
  - id: state
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/sysinfo_state.cpp
    title: src/sysinfo_state.cpp (SysInfo constructor, setWiFiInfo)
  - id: comms
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/comms.cpp
    title: src/comms.cpp (commInit, scanClients)
  - id: boards
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/scripts/boards.ps1
    title: scripts/boards.ps1
---

# Details

* `wifi_connect()` has `WiFi.config({IP_ADDR})` commented out, so the router assigns the address.[^net]
* The `SysInfo` constructor initializes the `IPAddress` fields `ipAddress` from `IP_ADDR` and `ipGateway` from `IP_GW`. `setWiFiInfo()` later updates the **string** copies (`strIpAddress`, `strGatewayIpAddress`) from DHCP, but never the `IPAddress` fields.[^state]
* `refIpAddress()` and `refGatewayIpAddress()` return those constants. They are used to:
  * build the static broadcast list on "this board's /24" and skip "this board" (`commInit`);
  * skip "this board" among mDNS-discovered hosts (`scanClients`);
  * ping "the gateway" (`wifi_connect`, `wifi_check`).[^comms]
* Addresses for Dev already disagree: `config.h` says 192.168.0.139, `scripts/boards.ps1` says 192.168.0.75. Tree matches at 192.168.0.182.[^boards]

# Impact

* While the router's reservation matches `IP_ADDR`, nothing goes wrong.
* If the DHCP address differs, a master can add **itself** as an mDNS-discovered recipient. Its broadcast then sends `PUT /fx` to its own web server from the same CORE0 task that serves HTTP, so the request cannot be answered and times out after 750 ms. This path is plausible from the code but not observed.
* If the router is not at 192.168.0.1, the health check pings the wrong host, fails every 7 minutes and forces a Wi-Fi reconnect.

# Resolution

Fixed in the working tree (after `6d11a87`, not yet committed):

* `setWiFiInfo()` now sets `ipAddress` and `ipGateway` from `wifi.localIP()` and `wifi.gatewayIP()`. `IP_ADDR`/`IP_GW` are only defaults until Wi-Fi connects.
* `wifi_connect()` records the Wi-Fi info **before** the first gateway ping, so that ping uses the DHCP gateway.
* DHCP stays: the router reserves 192.168.0.75 for the Dev board's MAC and 192.168.0.182 for Tree. `IP_ADDR` for Dev in `config.h` is now 192.168.0.75, matching `scripts/boards.ps1`; `scripts/upload_audio_seed.sh` also moved from .139 to .75.

The fix builds for both boards; not yet tested on a board.

[^net]: src/net_setup.cpp (wifi_connect)
[^state]: src/sysinfo_state.cpp (SysInfo constructor, setWiFiInfo)
[^comms]: src/comms.cpp (commInit, scanClients)
[^boards]: scripts/boards.ps1
