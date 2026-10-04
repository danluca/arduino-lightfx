---
type: Catalog
title: Effect catalog
description: All 50 registered effects, with registry index, ID, description, source file, default random-selection weight and holiday weight overrides.
tags: [effects, catalog, weights, holidays]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: fxsrc
    resource: https://github.com/danluca/arduino-lightfx/tree/6d11a87/src
    title: src/fxA.cpp … src/fxK.cpp (EffectInfo declarations and fxRegister)
  - id: efx
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/efx_setup.cpp
    title: src/efx_setup.cpp (categorySetup order)
---

# Catalog

The registry index comes from the registration order: categories A, B, C, D, E, F, H, I, J, K (`categorySetup[]`), and within each file the order of the `registerEffect` calls. Category K (`fxK.cpp`) registers nothing. A weight of 0 means the effect is never picked at random, but it can still be chosen manually.[^efx][^fxsrc]

| # | ID | Description | Default weight | Holiday overrides |
|---:|---|---|---:|---|
| 0 | FXA1 | Multiple Tetris segments | 3 | — |
| 1 | FXA2 | Randomly sized and spaced segments moving on entire strip | 10 | — |
| 2 | FXA3 | Moving variable dot size back and forth | 20 | — |
| 3 | FXA4 | Moving variable dot size back and forth with gradient background | 20 | — |
| 4 | FXA5 | Moving color swath on top of another | 20 | — |
| 5 | FXA6 | Sleep Light (`SleepLight` class; the sleep effect) | 0 | — (always excluded from random) |
| 6 | FXB1 | rainbow | 15 | Party 25, Valentine 30, StPatrick 0, Memorial 0, Independence 40, Thanksgiving 0, Christmas 30 |
| 7 | FXB2 | rainbow with glitter | 40 | Party 50, Valentine 50, StPatrick 0, Memorial 0, Independence 80, Thanksgiving 0, Christmas 50 |
| 8 | FXB3 | confetti B | 24 | Party 36, NewYear 36 |
| 9 | FXB4 | sinelon | 20 | — |
| 10 | FXB5 | juggle short segments | 30 | — |
| 11 | FXB6 | bpm | 20 | — |
| 12 | FXB7 | ease | 10 | — |
| 13 | FXB8 | fadein | 15 | — |
| 14 | FXB9 | juggle long segments | 14 | — |
| 15 | FXC1 | blend between two concurrent animations | 35 | — |
| 16 | FXC2 | blur function | 5 | — |
| 17 | FXC3 | Perlin Noise for moving up and down the strand | 4 | — |
| 18 | **FxC4** | lightnings | 9 | Halloween 20, every other holiday 0 (only appears at random during Halloween, or when the holiday is `None`) |
| 19 | FXC5 | matrix | 20 | Halloween 30, Christmas 10 |
| 20 | FXC6 | one sine | 20 | — |
| 21 | FXD1 | Confetti D | 21 | Party 30, NewYear 30 |
| 22 | FXD2 | dot beat | 20 | — |
| 23 | FXD3 | plasma | 24 | Halloween 36 |
| 24 | FXD4 | rainbow marching | 18 | Party 24, Valentine 0, StPatrick 0, Thanksgiving 0 |
| 25 | FXD5 | ripples | 64 | — |
| 26 | FXE1 | twinkle | 22 | Valentine 30, Christmas 40, NewYear 30, Halloween 0 |
| 27 | FXE2 | beat wave | 17 | — |
| 28 | FXE3 | sawtooth back/forth | 27 | — |
| 29 | FXE4 | serendipitous | 36 | — |
| 30 | FXE5 | three single color beat-waves | 42 | — |
| 31 | FXF1 | beat wave (overrides `windDown`) | 12 | — |
| 32 | FXF2 | Halloween breathe with various color blends | 32 | Halloween 42, Valentine 0, Christmas 0, NewYear 0 |
| 33 | FXF3 | Eye Blink | 32 | Halloween 42, Valentine 0, Christmas 0, NewYear 0 |
| 34 | FXF4 | Bouncy segments | 32 | Halloween 12 |
| 35 | FXF5 | Fireworks | 20 | Party 48, Memorial 48, Independence 80, Halloween 10, NewYear 80 |
| 36 | FXH1 | Fire segments (Fire2012 with palette) | 48 | Halloween 64, Thanksgiving 56, StPatrick 18, Valentine 10 |
| 37 | FXH2 | confetti H | 24 | — |
| 38 | FXH3 | filling the strand with colours | 18 | — |
| 39 | FXH4 | TwinkleFox | 12 | Valentine 20, Christmas 36, NewYear 24 |
| 40 | FXH5 | RainbowSparkle | 5 | Party 10, StPatrick 0, Halloween 0, Thanksgiving 0, Christmas 0 |
| 41 | FXH6 | JustSparkle | 0 | Halloween 25, NewYear 10 (holiday-only) |
| 42 | FXI1 | Ping Pong | 7 | — |
| 43 | FXI2 | Pacifica - gentle ocean waves | 9 | Memorial 16, Independence 16, Halloween 0, Thanksgiving 0, Christmas 0 |
| 44 | FXI3 | Bouncy Ball | 10 | — |
| 45 | FXI4 | Audio-seeded VU meter (see [FXI4 audio seeds](/effects/fxi4-audio-seeds.md)) | 12 | Party 24, NewYear 24 |
| 46 | FXI5 | Shore waves with backwash | 10 | Memorial 16, Independence 16, Halloween 0 |
| 47 | FXI6 | Bowling alley | 9 | — |
| 48 | FXJ1 | Popcorn (port of WS2812FX Popcorn by Keith Lord, MIT) | 16 | — |
| 49 | FXJ2 | Rain on a window (inspired by WS2812FX Rain, MIT) | 20 | Halloween 36, Valentine 0, Christmas 0 |

The effect code is identical to the RP2040 code line, so indexes, IDs and weights match across both. That keeps [broadcast](/network/multi-board-broadcast.md) between an RP2040 and an RP2350 board consistent, as long as both run the same registration order.

# Random-selection odds by holiday

Computed from the table above. Totals exclude FXA6.

| Holiday | Total weight | Eligible effects | Most likely (share of picks) |
|---|---:|---:|---|
| Party (default) | 1056 | 47 | FXD5 6.1%, FXB2 4.7%, FXF5 4.5% |
| ValentineDay | 865 | 43 | FXD5 7.4%, FXB2 5.8%, FXE5 4.9% |
| StPatrick | 856 | 43 | FXD5 7.5%, FXE5 4.9%, FXE4 4.2% |
| MemorialDay | 950 | 45 | FXD5 6.7%, FXF5 5.1%, FXH1 5.1% |
| IndependenceDay | 1102 | 47 | FXB2 7.3%, FXF5 7.3%, FXD5 5.8% |
| Halloween | 1007 | 45 | FXD5 6.4%, FXH1 6.4%, FXE5 4.2% |
| Thanksgiving | 885 | 42 | FXD5 7.2%, FXH1 6.3%, FXE5 4.7% |
| Christmas | 923 | 42 | FXD5 6.9%, FXB2 5.4%, FXH1 5.2% |
| NewYear | 1023 | 46 | FXF5 7.8%, FXD5 6.3%, FXH1 4.7% |

# Category files

| Category | Files | Notes |
|---|---|---|
| A | `fxA.h/.cpp` | Segment and dot movers, plus the sleep light. Shares `FxA::szStack` |
| B | `fxB.h/.cpp` | Classic FastLED DemoReel-style patterns |
| C | `fxC.h/.cpp` | Blends, blur, noise, lightning, matrix, sine |
| D | `fxD.h/.cpp` | Confetti, dot beat, plasma, rainbow march, ripples |
| E | `fxE.h/.cpp` | Twinkle and beat-wave family |
| F | `fxF.h/.cpp` | Halloween breathe and eyes, bouncy segments, fireworks |
| H | `fxH.h/.cpp` | Fire2012, TwinkleFox, sparkles |
| I | `fxI.h/.cpp` | Ping pong, Pacifica, bouncy ball, VU meter (seed-file driven), shore waves, bowling |
| J | `fxJ.h/.cpp` | Popcorn, rain (newest, 2026-10) |
| K | `fxK.h/.cpp` | Empty placeholder |

[^fxsrc]: src/fxA.cpp … src/fxK.cpp (EffectInfo declarations and fxRegister)
[^efx]: src/efx_setup.cpp (categorySetup order)
