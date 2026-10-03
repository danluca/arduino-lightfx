# Effects

* [LED effect lifecycle](led-effect-lifecycle.md) - The `LedEffect` base class: its five-state machine, `setup`/`run`/`windDown`/`cleanup` hooks, and the AT/FROM timeline macros.
* [Effect registry](effect-registry.md) - Registration, registry indexes, holiday-weighted random selection, auto-roll, sleep handling and deferred effect creation.
* [Effect catalog](effect-catalog.md) - All 50 registered effects with their registry index, ID, description, default weight and holiday overrides.
* [Effect transitions](effect-transitions.md) - The six turn-off animations that play while an effect winds down.
* [Shared FX state](shared-fx-state.md) - Globals and buffers that effects share, and what `resetGlobals()` restores.
* [Adding an effect](adding-an-effect.md) - Step-by-step playbook for writing and registering a new effect.
* [FXI4 audio seeds](fxi4-audio-seeds.md) - How the VU-meter effect's rhythm seed files are generated, uploaded and loaded.
