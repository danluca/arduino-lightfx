---
type: Architecture
title: Multi-board broadcast
description: A board with broadcast enabled becomes the master. On every effect transition it sends PUT /fx with the new effect index to the other boards, so the house lights change roughly in sync.
tags: [broadcast, sync, multi-board, comms]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: comms
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/comms.cpp
    title: src/comms.cpp
  - id: config
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/include/config.h
    title: include/config.h (STATIC_BROADCAST_CLIENTS)
---

# Roles

* **Master:** `fxBroadcastEnabled = true`. Set it with `PUT /fx {"broadcast":true}` or the UI checkbox. It is saved in `/state.json`. When broadcast is turned on, the master immediately broadcasts its current effect.
* **Follower:** receives `PUT /fx {"effect":N,"auto":false,"broadcast":false,"source":"<master device name>"}`. It switches to effect N, **turns auto-roll off**, makes sure it is not a master itself, and stores `masterBoardName`.[^comms]

# Recipients

* Static list: the last IP octets in `STATIC_BROADCAST_CLIENTS` (`10, 11, 12`), combined with this board's own /24 subnet. The board skips its own address. At most 10 recipients (`FixedQueue<unique_ptr<BroadcastClient>,10>`).[^config]
* Each client has `online`, `active` and `static` flags. `scanClients` (every 15 min, mDNS builds only, while broadcasting) pings each one. Static clients that do not answer are marked offline but kept. Non-static clients are removed.
* A follower can opt out by replying with `"talkToHand": true`. The master then marks it inactive.

# Flow

1. The FX task's `transitionEffect()` calls `postFxChangeEvent(index)`, which posts `FX_SYNC` to `bcQueue` once comms is configured. Followers post it too, but `fxBroadcast` returns early when broadcast is disabled.
2. On CORE0, `fxBroadcast(index)` checks Wi-Fi, the effect ID and the master flag, then calls `clientUpdate` for each recipient.
3. `clientUpdate` uses `ArduinoHttpClient` with a 1 s connect timeout and a 2 s response timeout, sends `Connection: close`, parses the JSON response, updates the flags, and waits 200 ms between clients.

The broadcast is posted when the old effect **starts** winding down. Each board then plays its own transition, so the switch is close to simultaneous rather than exact.

# Constraints

* Effects are identified by **registry index**. All boards must run firmware with the same registration order. See [adding an effect](/effects/adding-an-effect.md).
* `IGNORE_WEB_EFFECT_CHANGES` was meant to make a board ignore non-UI effect changes. It currently does not compile. See [ignore-web-flag compile error](/issues/ignore-web-flag-compile-error.md).
* `/status.json` → `master` shows `active`, `activeClients`, `knownClients` and `masterBoard`.

[^comms]: src/comms.cpp
[^config]: include/config.h (STATIC_BROADCAST_CLIENTS)
