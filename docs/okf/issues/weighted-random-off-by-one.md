---
type: Issue
title: Weighted-random off-by-one
description: nextRandomEffectPos drew random16(total+1), so a draw equal to the total matched no effect and the scheduled switch was skipped. Fixed with random16(total).
tags: [bug, effects, random, registry]
severity: low
issue_state: fixed
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: reg
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/EffectRegistry.cpp
    title: src/EffectRegistry.cpp (nextRandomEffectPos)
---

# Defect (as introduced with holiday weighting in `6a6eba9`)

```cpp
uint16_t rnd = random16(totalSelectionWeight+1);   // range 0..total
```

The cumulative-weight walk finds an effect only when `rnd < total`. When `rnd == total` (probability `1/(total+1)`, about 0.1% with totals around 850–1100), the loop matches nothing, `desiredEffectIndex` stays unchanged, and the 7-minute switch is silently skipped. The log still says "Random effect selection … index N" with the current index.[^reg]

# State

Fixed in `6d11a87`: the draw is now `random16(totalSelectionWeight)`, range `0..total-1`, so every draw selects an effect.

[^reg]: src/EffectRegistry.cpp (nextRandomEffectPos)
