# Defects

* [Mic queue item size](mic-queue-item-size.md) - (fixed) `micQueue` items were pointer-sized but the code sends an 8-byte struct, so web audio-threshold updates set the threshold to 0.
* [Ignore-web flag compile error](ignore-web-flag-compile-error.md) - (fixed) Building with `IGNORE_WEB_EFFECT_CHANGES=1` (`build.ps1 -ignoreBroadcast`) failed because `isUi` was commented out.
* [Cross-task effect registry access](cross-task-registry-access.md) - (fixed) The ALM task called into the effect registry while the FX task was running it on the other core.

# Risks

* [Hard-coded upload token](hardcoded-upload-token.md) - (mitigated) The upload token now lives in the git-ignored `include/secrets.h`; the current value is still in git history until rotated.

# Documentation

* [Documentation drift](documentation-drift.md) - (fixed) Places where README, AGENTS.md, assistant instruction files and code comments disagreed with the code.
