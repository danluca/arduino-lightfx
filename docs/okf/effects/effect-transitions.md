---
type: Architecture
title: Effect transitions
description: EffectTransition (transEffect) has six turn-off animations, with direction variants, that play during an effect's WindDown state.
tags: [effects, transitions, winddown]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: trans
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/transition.cpp
    title: src/transition.cpp
  - id: reg
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/EffectRegistry.cpp
    title: src/EffectRegistry.cpp (transitionEffect)
---

# How it is used

`EffectRegistry::transitionEffect()` calls `transEffect.prepare(random8())`. The default `LedEffect::windDown()` then calls `transEffect.transition()` every loop until it returns true, meaning all LEDs are off. Then the old effect cleans up and is deleted.[^reg]

`prepare(selector)` picks `sel = selector % (count*2)`. The low bit chooses a direction variant. Bits 8–15, if set, force a preferred animation, but because the registry passes a `random8()` value those bits are always 0.[^trans]

# Turn-off animations

| # | Method | Behavior | Variant bit |
|---|---|---|---|
| 0 | `offSpots` | Fades random spots, in growing groups taken from `turnOffSeq` (1, 1, 2, 2, 2, 3 … 10), following `stripShuffleIndex`. Steps every 30 ms | — |
| 1 | `offWipe` | Shifts the whole strip with black fed in. Steps every 60 ms | right / left |
| 2 | `offFade` | `fadeToBlackBy(32)` every 50 ms | — |
| 3 | `offSplit` | Blends the two halves to black, from the center outward or from the ends inward | outward / inward |
| 4 | `offRandomBars` | Splits the strip into random 3–9 px bars and spreads black across each one | right / left |
| 5 | `offHalfWipe` | Shifts the two halves inward or outward | inward / outward |

Most variants poll `isAnyLedOn` every 500–720 ms to decide when they are done. `offSplit` and `offRandomBars` decide from their own progress.[^trans]

# Notes

* An effect can override `windDown()` for a custom exit. FXF1 does this.
* Transitions call `FastLED.show(stripBrightness)` directly. They run on whichever task drives the registry, normally FX.
* `randomBarSegs` is generated once (on the first `prepare`) and then reused.

[^trans]: src/transition.cpp
[^reg]: src/EffectRegistry.cpp (transitionEffect)
