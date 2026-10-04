---
type: Business Rule
title: Brightness dimming
description: Strip brightness is lowered in steps from 22:00 to 06:00 unless the user locked a fixed brightness through the web API.
tags: [brightness, dimming, schedule]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: fxutil
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/fxutil.cpp
    title: src/fxutil.cpp (adjustStripBrightness)
  - id: efx
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/efx_setup.cpp
    title: src/efx_setup.cpp (fx_run, STRIP_BRIGHTNESS)
---

# Rule

`adjustStripBrightness()` runs every 30 s on the FX task. It is skipped when `stripBrightnessLocked` is set or Wi-Fi is down.[^fxutil]

| Local hour | Scale (out of 255) | Result |
|---|---|---|
| 06:00 – 21:59 | none | `FastLED.getBrightness()` (255) |
| 22:00 – 22:59 | 204 (80%) | `dim8_raw(scale8(255, 204))` |
| 23:00 – 23:59 | 178 (70%) | `dim8_raw(scale8(255, 178))` |
| 00:00 – 05:59 | 152 (60%) | `dim8_raw(scale8(255, 152))` |

`dim8_raw` applies a gamma-like curve, so the perceived output is much lower than the percentages suggest. The function's comments now match these numbers.

# Web override

* `PUT /fx {"brightness": N}` with N > 0 sets `stripBrightness = N` and **locks** it, which turns off time-of-day dimming.
* `PUT /fx {"brightness": 0}` unlocks and immediately applies the time-based value.
* The value is saved in `/state.json` as `stripBrightness`. `/status.json` reports `fx.brightness` and `fx.brightnessLocked`.[^efx]

All effects and transitions pass `stripBrightness` to `FastLED.show()`. `FastLED.setBrightness` itself stays at 255 (`BRIGHTNESS` in `config.h`).

[^fxutil]: src/fxutil.cpp (adjustStripBrightness)
[^efx]: src/efx_setup.cpp (fx_run, STRIP_BRIGHTNESS)
