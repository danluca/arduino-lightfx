---
type: Issue
title: Effect ID inconsistency
description: Effect FxC4 (lightnings) breaks the uppercase FX<cat><n> ID convention, and registry lookups by ID return index 0 (FXA1) on a miss instead of an error.
tags: [risk, effects, registry, naming]
severity: low
issue_state: open
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: fxc
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/fxC.cpp
    title: src/fxC.cpp (fxc4Desc)
  - id: reg
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/EffectRegistry.cpp
    title: src/EffectRegistry.cpp (findEffectIndex, nextEffectPos(const char*))
---

# Details

* `fxc4Desc` has `.id = "FxC4"`. All 49 other IDs are uppercase (`FXC4` would be expected).[^fxc]
* `findEffectIndex(id)` and `nextEffectPos(const char*)` use `strcmp`, so they are case-sensitive, and they **return 0 when nothing matches**. Looking up `"FXC4"` would quietly select FXA1 (index 0).[^reg]

# Current impact

Low. The web UI, broadcast and saved state all use registry **indexes**. The only ID lookup is for the sleep effect (`"FXA6"`). The mixed case does show up in `/config.json`, `pastEffects` and logs.

# Suggested fix

Rename the ID to `FXC4`. Consider returning a sentinel such as `UINT16_MAX` from `findEffectIndex` when there is no match.

[^fxc]: src/fxC.cpp (fxc4Desc)
[^reg]: src/EffectRegistry.cpp (findEffectIndex, nextEffectPos(const char*))
