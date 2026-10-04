---
type: Business Rule
title: Holidays and palettes
description: How the current date maps to a holiday theme (by month and day), which palettes each theme loads, and how auto and manual theme selection interact.
tags: [holidays, palettes, colors, scheduling]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: time
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/timeutil.cpp
    title: src/timeutil.cpp (buildHoliday)
  - id: pal
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/PaletteFactory.cpp
    title: src/PaletteFactory.cpp
---

# Date ranges (as implemented)

`buildHoliday(time)` encodes the date as `(month << 8) | day` and compares it with **strict** bounds.[^time]

| Holiday | Effective dates |
|---|---|
| ValentineDay | Feb 12 – Feb 15 |
| StPatrick | Mar 15 – Mar 18 |
| MemorialDay | May 25 – May 31 |
| IndependenceDay | Jul 1 – Jul 5 |
| Halloween | Oct 1 – Nov 3 |
| Thanksgiving | Nov 4 – Nov 30 |
| Christmas | Dec 23 – Dec 27 |
| NewYear | Dec 31 – Jan 2 |
| Party | every other day |

Because the bounds are strict (`md > 0x020B`, `md > 0xC1E`), Feb 11 and Dec 28–30 fall into **Party**. The code comments now say the same. `None` is not a date-derived theme; it means "no preference".

# Auto and manual selection

* `paletteFactory.setHoliday(h)`: `None` turns auto mode **on** and recomputes the holiday from the date. Any other value turns auto mode **off** and fixes the theme.
* `adjustHoliday()` changes nothing while auto mode is off **or Wi-Fi is down**, so the saved holiday from `/state.json` stays in effect until the network comes up.
* An ALM timer re-evaluates the holiday every 12 h (`HOLIDAY_UPDATE`). It is also re-evaluated after a successful NTP sync.
* `PUT /fx {"holiday":"Halloween"}` → `COLOR_THEME` → FX task. The holiday also changes [effect selection weights](/effects/effect-catalog.md).[^pal]

# Palettes per theme

Effects get these through `resetGlobals()`: `palette = mainPalette()`, `targetPalette = secondaryPalette()`.

| Theme | Main palette | Secondary palette |
|---|---|---|
| Halloween | `HalloweenColors_p` (dark red, orange, purple, black) | `HalloweenStripeColors_p` |
| Thanksgiving | `HeatColors_p` | `LavaColors_p` |
| Christmas | `ChristmasColors_p` (green, red, white) | `RainbowColors_p` |
| NewYear | `Rainbow_gp` | `OceanColors_p` |
| MemorialDay, IndependenceDay | `PatrioticColors_p` | `PatrioticColors_p` |
| ValentineDay | `ValentineColors_p` | `LavaColors_p` |
| StPatrick | `PatrickColors_p` (greens) | `ForestColors_p` |
| Party (default) | `PartierColors_p` | `PartierColorsCompl_p` |

Other palettes are available through `selectRandomPalette()` and `selectNextPalette()`: `SleepyColors_p`, `RedGreenWhite_p`, `FairyLight_p`, `Snow_p`, `RetroC9_p`, `Ice_p`. `randomPalette(ofsHue)` builds a random four-color HSV palette. `isHolidayLimitedHue()` is true only for Halloween.[^pal]

[^time]: src/timeutil.cpp (buildHoliday)
[^pal]: src/PaletteFactory.cpp
