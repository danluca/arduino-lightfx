---
type: Issue
title: Ignore-web flag compile error
description: With IGNORE_WEB_EFFECT_CHANGES=1, handlePutConfig refers to isUi, but its declaration is commented out. build.ps1 -ignoreBroadcast and ota_upgrade.ps1 -ignoreBroadcast therefore fail to compile.
tags: [bug, build, web, broadcast]
severity: medium
issue_state: fixed
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: web
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/web_server.cpp
    title: src/web_server.cpp (lines ~284–386)
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/scripts/util.ps1
    title: scripts/util.ps1 (prepEnvironment)
---

# Defect

In `web::handlePutConfig`, the origin checks are commented out:

```cpp
// const bool isUi = xSource.equalsIgnoreCase(kXSourceUi);
// const bool isBoard = ...;
```

The three `#if IGNORE_WEB_EFFECT_CHANGES == 1` blocks (for `auto`, `effect` and `broadcast`) still contain `if (!isUi) { ... }`. With the flag at its default of 0 this code is not compiled. With the flag set to 1 it fails with "'isUi' was not declared in this scope".[^web]

# Who hits it

`build.ps1 -ignoreBroadcast` and `ota_upgrade.ps1 -ignoreBroadcast` add `-DIGNORE_WEB_EFFECT_CHANGES=1`.[^util] The feature was introduced on the RP2040 line in `1284316`/`1863cf2` (2025-12) to let a board ignore effect changes broadcast from other boards. `/status.json` still reports `fx.ignoreWebFx`.

# Fix

Restore `const bool isUi = xSource.equalsIgnoreCase(kXSourceUi);` (the `userAgent` and `xSource` locals still exist), or remove the dead feature, the script switch and the status field together.

# Resolution

Fixed on the RP2040 line in `d5de0ba` and ported here in `6d11a87`: the `isUi` declaration is restored, guarded by `#if IGNORE_WEB_EFFECT_CHANGES == 1` to avoid an unused-variable warning in default builds.

# Verification status

On the RP2040 line, `rp2040-rel` builds both with and without the flag. On this board the fix was checked by reading `src/web_server.cpp`; an `rp2350-rel -ignoreBroadcast` build was not run during this review.

[^web]: src/web_server.cpp (lines ~284–386)
[^util]: scripts/util.ps1 (prepEnvironment)
