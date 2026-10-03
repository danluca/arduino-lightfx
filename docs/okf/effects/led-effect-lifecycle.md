---
type: Architecture
title: LED effect lifecycle
description: The LedEffect base class, its Idle/Setup/Running/WindDown/Cleanup state machine, its virtual hooks, and the AT/FROM time-code macros.
tags: [effects, state-machine, lifecycle, ledeffect]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: ledh
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/include/LedEffect.h
    title: include/LedEffect.h
  - id: ledcpp
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/LedEffect.cpp
    title: src/LedEffect.cpp
---

# States

| State | Kind | What happens each `loop()` |
|---|---|---|
| `Idle` | stable | Nothing. The registry deletes the object when it sees `Idle` |
| `Setup` | transient (1 cycle) | Calls `setup()` (default: `resetGlobals()`), then `restartPerformance()`, then advances to `Running` |
| `Running` | stable | `updateTimeCode()`, then `run()` |
| `WindDown` | transient | Calls `windDown()` (default: `transEffect.transition()`) until it returns true, then advances to `Cleanup` |
| `Cleanup` | transient (1 cycle) | Calls `cleanup()` (default: no-op), then advances to `Idle` |

The natural progression is Idle → Setup → Running → WindDown → Cleanup → Idle.[^ledcpp]

# Driving the state machine

`desiredState(dst)` takes **one step** toward `dst` using a transition table. It does not jump straight to `dst`:

* Idle + desire Running → Setup. The next `loop()` then runs setup and lands in Running.
* Running + desire Idle → WindDown. The transition then plays and the effect goes through Cleanup to Idle.
* WindDown + desire Running → Running. This cancels a wind-down in progress.

Pass stable targets (`Running`, `Idle`). A transient target only lands the effect in the next stable state on the path.[^ledcpp]

# Hooks to override

| Hook | Required | Typical use |
|---|---|---|
| `run()` | yes (pure virtual) | Draw one frame. Usually wrapped in `EVERY_N_MILLISECONDS(...)`, ending with `FastLED.show(stripBrightness)` |
| `setup()` | no | Call `LedEffect::setup()` first (it resets the globals), then set up effect-specific state and palettes |
| `windDown()` | no | Custom fade-out. Return true when the strip is dark. `FxF1` overrides it |
| `cleanup()` | no | Free dynamic buffers (vectors, `new` allocations) |
| `baseConfig(JsonObject&)` | no | Add effect-specific fields to its JSON description |

# Timeline macros (AT / FROM)

Adapted from Mark Kriegsman's "Time Performance". `timeCode` is the number of ms since `Setup` (`restartPerformance()`).[^ledh]

```cpp
FROM(0,0,0)    { drawRed(); }     // runs every loop from t=0 until the next FROM fires
FROM(0,0,5.0)  { drawBlue(); }    // runs every loop from t=5s on
AT(0,0,10.0)   { flash(); }       // runs exactly once at t>=10s
```

At a boundary, both neighboring `FROM` blocks run in the same loop.

# Identity

Each instance keeps a reference to its static `EffectDescription` (`id`, `description`) and its registry index. `name()` returns the short ID (for example `FXJ2`). The destructor logs "Effect %s signing off - Sayonara!".

[^ledh]: include/LedEffect.h
[^ledcpp]: src/LedEffect.cpp
