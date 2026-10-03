---
type: Reference
title: Shared FX state
description: Globals and buffers shared by effects (some atomic, read by other tasks), the defaults that resetGlobals() restores, and the fxutil helper library.
tags: [effects, globals, fastled, fxutil]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: efxh
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/include/efx_setup.h
    title: include/efx_setup.h
  - id: efx
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/efx_setup.cpp
    title: src/efx_setup.cpp (resetGlobals)
  - id: fxutil
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/include/fxutil.h
    title: include/fxutil.h
---

# Cross-task globals (outside the `fx` namespace)

| Variable | Type | Writers | Readers |
|---|---|---|---|
| `stripBrightness` | `std::atomic<uint8_t>` | FX (time-of-day dimming, web brightness) | everyone: passed to `FastLED.show()` |
| `stripBrightnessLocked` | atomic bool | FX (true when the web sets a non-zero brightness) | web status |
| `fxBump` | atomic bool | Mic (audio peak above threshold) | FX (every 30 s → next effect) |
| `totalAudioBumps` | atomic u16 | FX | web status |
| `audioBumpThreshold` | atomic u16 (default 5000) | Mic and state restore | Mic, web |
| `fxBroadcastEnabled` | atomic bool | Comms, state restore | FX, Comms, web |
| `speed`, `curPos` | atomic u16 | effects | effects |
| `brightness`, `colorIndex`, `fade`, `hue`, `delta`, `saturation`, `dotBpm`, `hueDiff` | `volatile` | effects | effects |

# FX-only globals (`namespace fx`)

`leds`, `ledSet`, `tpl`, `others`, `frame`, `stripShuffleIndex`, `palette`, `targetPalette`, `mode` (`TurnOff`/`Chase`), `dirFwd`, `dist`, and `transEffect`. A January 2026 refactor (`9d2dffa`) grouped these here to mark them as single-task. See [memory model](/architecture/memory-model.md) for sizes.[^efx]

# `resetGlobals()` defaults (run by `LedEffect::setup()`)

`FastLED.clear` (flushed only when NTP is OK and the board is awake), `setBrightness(255)`, clear `frame`, `palette = paletteFactory.mainPalette()`, `targetPalette = secondaryPalette()`, `mode = Chase`, `brightness = 224`, `colorIndex = lastColorIndex = 0`, `curPos = 0`, `speed = 100`, `fade = 8`, `hue = 50`, `delta = 1`, `saturation = 100`, `dotBpm = 30`, `hueDiff = 256`, `dist = 1`, `dirFwd = true`, `fxBump = false`.

If you add a shared global, add it to `resetGlobals()` too. Its doc comment says it "needs to account for ALL global variables".[^efx]

# fxutil helpers (`namespace fx`)

| Group | Functions |
|---|---|
| Shifting | `shiftRight`, `shiftLeft`, `loopRight` (with an optional `Viewport`) |
| Replication | `replicateSet(tpl, others)`, `replicateMirrorSet`, `mirrorLow`, `mirrorHigh`, `copySet`, `copySubSet`, `fillSet` |
| Blending | `spreadColor`, `moveBlend`, `rblend`, `blendMultiply`, `blendScreen`, `blendOverlay` |
| Easing | `easeOutBounce`, `easeOutQuad` |
| Queries | `isAnyLedOn`, `countPixelsBrighter`, `areSame`, `getBrightness` |
| Random | `shuffleIndexes`, `shuffle` |
| Brightness | `adjustStripBrightness()`. See [brightness dimming](/scheduling/brightness-dimming.md) |

Global macros in `global.h`: `capu`/`capd`/`capr` (clamp), `inr` (in range), `inc`/`incr` (modular increment), `qsuba`/`qsubd`, `asub`, `arrSize`. Plus `cadd8`/`csub8` for byte math that saturates at a cap.

[^efxh]: include/efx_setup.h
[^efx]: src/efx_setup.cpp (resetGlobals)
[^fxutil]: include/fxutil.h
