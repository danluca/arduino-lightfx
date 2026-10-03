---
type: Playbook
title: Adding an effect
description: Steps to implement, register, weight and verify a new LedEffect without breaking registry indexes or the heap.
tags: [effects, howto, contributing]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: fxj
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/fxJ.cpp
    title: src/fxJ.cpp (most recent example)
  - id: readme
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/README.md
    title: README.md (Contributing)
    author: human:danluca
---

# Steps

1. **Pick a category.** Add the effect to an existing `fxX.h/.cpp` pair, or fill the empty K category. Appending at the **end** of a category, or in the last category, keeps every existing registry index the same. Inserting in the middle shifts the indexes of all later effects, which changes saved `curFx` values and the indexes other boards broadcast. See [effect registry](/effects/effect-registry.md).
2. **Declare the class** in the header, inside the category namespace:
   ```cpp
   class FxJ3 : public LedEffect {
   public:
       FxJ3();
       void setup() override;
       void run() override;
       void cleanup() override;   // only if you allocate
   private:
       // effect state; prefer fixed-size members over heap buffers
   };
   ```
3. **Declare its identity and weights** at the top of the `.cpp`:
   ```cpp
   static constexpr HolidayWeight fxj3HolidayWeights[] = {
       {.holiday = Halloween, .weight = 30},
       {.holiday = Christmas, .weight = 0},
   };
   static const EffectInfo fxj3Desc = {.factory = EFFECT_FACTORY(FxJ3),
       .desc = {.id = "FXJ3", .description = "short description"},
       .selectionWeight = 15, HOLIDAY_WEIGHTS(fxj3HolidayWeights)};
   FxJ3::FxJ3() : LedEffect(fxj3Desc) {}
   ```
   Use an uppercase `FX<cat><n>` ID that no other effect uses. Lookups are case-sensitive.
4. **Register it** by appending `fxRegistry.registerEffect(&fxj3Desc);` to the category's `fxRegister()`.
5. **Implement `setup()`.** Call `LedEffect::setup()` first; it runs `resetGlobals()` and loads the holiday palettes. Then set up your state. Use `paletteFactory.isHolidayLimitedHue()` if your colors should respect Halloween limits.
6. **Implement `run()`.** Throttle with `EVERY_N_MILLISECONDS(n)`. Draw into `tpl`/`frame` and replicate with `replicateSet` or `replicateMirrorSet` when the pattern repeats along the strip. End each frame with `FastLED.show(stripBrightness)` so dimming and sleep brightness apply. Never block: the FX task feeds the [watchdog](/architecture/watchdog-and-health.md) and must keep looping.
7. **Free memory in `cleanup()`.** Delete anything you allocated in `setup()`. The registry deletes the object after `Idle`, but freeing earlier keeps peak heap use low.
8. **Build and test on hardware** (see the [build and deploy](/build/build-and-deploy.md) playbook). Select the effect from the web UI, then check `/stats.html` (`minHeap`, `freeBlocks`, task stack high-water marks) before and after several transitions. Look for steady heap use and no flicker.[^readme]
9. **Update this bundle.** Add a row to the [effect catalog](/effects/effect-catalog.md) and an entry to the bundle `log.md`.

# Checklist

- [ ] ID is unique and uppercase.
- [ ] Appended, so no existing indexes moved.
- [ ] Weight and holiday overrides are chosen deliberately. A weight of 0 everywhere means the effect is manual-only.
- [ ] No blocking calls, no `delay()`.
- [ ] Every allocation in `setup()` is freed in `cleanup()`.
- [ ] No stack arrays sized by `NUM_PIXELS` (the FX stack is 1536 B).

[^fxj]: src/fxJ.cpp (most recent example)
[^readme]: README.md (Contributing)
