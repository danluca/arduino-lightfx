---
type: Issue
title: Weighted-random off-by-one
description: nextRandomEffectPos drew random16(total+1), so a draw equal to the total matched no effect and the scheduled switch was skipped. The fix (random16(total)) is in the working tree but not committed.
tags: [bug, effects, random, registry]
severity: low
issue_state: fixed-uncommitted
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2026-11-03T00:00:00Z
sources:
  - id: reg
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/EffectRegistry.cpp
    title: src/EffectRegistry.cpp (nextRandomEffectPos) at 73c7243 and in the working tree
---

# Defect (at commit 73c7243)

```cpp
uint16_t rnd = random16(totalSelectionWeight+1);   // range 0..total
```

The cumulative-weight walk finds an effect only when `rnd < total`. When `rnd == total` (probability `1/(total+1)`, about 0.1% with totals around 850–1100), the loop matches nothing, `desiredEffectIndex` stays unchanged, and the 7-minute switch is silently skipped. The log still says "Random effect selection … index N" with the current index.[^reg]

# State

The working tree changes this to `random16(totalSelectionWeight)`, which is correct. Commit it together with the holiday-weighting feature (`73c7243`). After that, set this concept's `issue_state` to `fixed`, or mark it `status: deprecated`.

[^reg]: src/EffectRegistry.cpp (nextRandomEffectPos) at 73c7243 and in the working tree
