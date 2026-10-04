---
type: Architecture
title: Multi-board broadcast
description: A board with broadcast enabled becomes the master. On every effect transition it sends PUT /fx with the new effect index to the other boards, found from a static IP list and mDNS discovery, so the lights change roughly in sync.
tags: [broadcast, sync, multi-board, comms, mdns]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: comms
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/comms.cpp
    title: src/comms.cpp
  - id: net
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/net_setup.cpp
    title: src/net_setup.cpp (mDNS discovery)
  - id: config
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/config.h
    title: include/config.h (STATIC_BROADCAST_CLIENTS)
---

# Roles

* **Master:** `fxBroadcastEnabled = true`. Set it with `PUT /fx {"broadcast":true}` or the UI checkbox. It is saved in `/state.json`. When broadcast is turned on, the master immediately broadcasts its current effect.
* **Follower:** receives `PUT /fx {"effect":N,"auto":false,"broadcast":false,"source":"<master device name>"}`. It switches to effect N, **turns auto-roll off**, makes sure it is not a master itself, and stores `masterBoardName`.[^comms]

# Recipients

* **Static list:** the last IP octets in `STATIC_BROADCAST_CLIENTS` (`10, 11, 12`), combined with this board's /24 subnet. These are the RP2040 boards' addresses; neither RP2350 board (`Dev`, `Tree`) is in the list. The board skips its own address. At most 10 recipients (`FixedQueue<unique_ptr<BroadcastClient>,10>`).[^config]
* **mDNS discovery:** every `lightfx-*` host found through the `_lucasfx._tcp` service query is added as a non-static recipient by `scanClients` (every 15 min, mDNS builds only). See [Wi-Fi and mDNS](/network/wifi-and-mdns.md).[^net]
* Each client has `online`, `active` and `static` flags. While broadcasting, `scanClients` pings each recipient. Static clients that do not answer are marked offline but kept. Non-static clients that are neither discovered nor reachable are removed.
* A follower can opt out by replying with `"talkToHand": true`. The master then marks it inactive.
* "Own address" is the DHCP address reported by the Wi-Fi stack, so a board never lists itself as a recipient.

# Flow

1. The FX task's `transitionEffect()` calls `postFxChangeEvent(index)`, which posts `FX_SYNC` to `bcQueue` once comms is configured. Followers post it too, but `fxBroadcast` returns early when broadcast is disabled.
2. On CORE0, `fxBroadcast(index)` checks Wi-Fi, the effect ID and the master flag, then calls `clientUpdate` for each recipient.
3. `clientUpdate` uses the framework `HTTPClient` with a **750 ms** timeout (kept short so CORE0 does not starve the watchdog), sends `Connection: close`, parses the JSON response, updates the flags, and waits 100 ms between clients.

The broadcast is posted when the old effect **starts** winding down. Each board then plays its own transition, so the switch is close to simultaneous rather than exact.

# Constraints

* Effects are identified by **registry index**. All boards, including RP2040 boards, must run firmware with the same registration order. See [adding an effect](/effects/adding-an-effect.md).
* `IGNORE_WEB_EFFECT_CHANGES=1` (`build.ps1 -ignoreBroadcast`) makes a board ignore effect, auto and broadcast changes that do not come from the UI.
* `/status.json` → `master` shows `active`, `activeClients`, `knownClients` and `masterBoard`.

[^comms]: src/comms.cpp
[^net]: src/net_setup.cpp (mDNS discovery)
[^config]: include/config.h (STATIC_BROADCAST_CLIENTS)
