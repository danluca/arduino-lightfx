---
type: Issue
title: Audio remnants
description: The Plasma 2350 W has no microphone, but microphone-era types, enum values and web UI chart code from the RP2040 line are still in the source. They are dead code, not a functional defect.
tags: [cleanup, audio, dead-code]
severity: low
issue_state: fixed
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: taskmsg
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/task_msg.h
    title: include/task_msg.h
  - id: pixeljs
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/www/pixel.js
    title: www/pixel.js
  - id: pixelcss
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/www/pixel.css
    title: www/pixel.css
  - id: reg
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/EffectRegistry.cpp
    title: src/EffectRegistry.cpp (nextEffectPos)
---

# What is left

| Where | Remnant |
|---|---|
| `include/task_msg.h` | `enum MikeAction { AUDIO_THRESHOLD_UPDATE }` and `struct AudioActionMessage`. Nothing creates a queue for them[^taskmsg] |
| `include/task_msg.h` | `FxAction` values `AUDIO_THRESHOLD` and `AUDIO_CHANGE`. `fx_run` has no case for them and would log "Fx Action not supported" |
| `www/pixel.js` | `histOptions`, an "Audio Levels" CanvasJS column chart definition that nothing renders[^pixeljs] |
| `www/pixel.css` | `#audioLevelHistogram` and `#audioHistogram` styles[^pixelcss] |
| `src/EffectRegistry.cpp` | `nextEffectPos()` (sequential next effect), which only the RP2040 microphone bump called[^reg] |
| README.md | "Audio Reactive: PDM microphone integration" in the feature list. See [documentation drift](/issues/documentation-drift.md) |

The effect `FXI4` ("Audio-seeded VU meter") is **not** a remnant: it replays seed files and needs no microphone. See [FXI4 audio seeds](/effects/fxi4-audio-seeds.md).

# Resolution

Removed in the working tree (after `6d11a87`, not yet committed): `MikeAction`, `AudioActionMessage`, the `AUDIO_THRESHOLD`/`AUDIO_CHANGE` values of `FxAction`, `EffectRegistry::nextEffectPos()`, `histOptions` from `www/pixel.js`, the audio histogram and CanvasJS credit styles from `www/pixel.css`, and the CanvasJS `<script>` from `www/index.html` (only the audio chart used it; `stats.html` still loads it). The generated `include/pixel_js.h`, `pixel_css.h` and `index_html.h` were edited the same way. README.md no longer claims audio-reactive effects.

Porting note: `FxAction` values shifted. They are only used in memory between tasks of the same build, so nothing persisted or sent between boards changes. Ports from the RP2040 line that touch `task_msg.h` or the web UI need to skip these parts.

[^taskmsg]: include/task_msg.h
[^pixeljs]: www/pixel.js
[^pixelcss]: www/pixel.css
[^reg]: src/EffectRegistry.cpp (nextEffectPos)
