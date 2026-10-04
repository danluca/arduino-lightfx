# Defects

* [Wi-Fi time fallback](wifi-gettime-fallback.md) - (fixed) When NTP failed, the clock was set from `WiFi.getTime()`, which returns `millis()` on this board, so time jumped to the early 1970s until NTP succeeded.
* [Ignore-web flag compile error](ignore-web-flag-compile-error.md) - (fixed) Building with `IGNORE_WEB_EFFECT_CHANGES=1` (`build.ps1 -ignoreBroadcast`) failed because `isUi` was commented out.
* [Cross-task effect registry access](cross-task-registry-access.md) - (fixed) The ALM task called into the effect registry while the FX task was running it on the other core.
* [update.ps1 OTA env bug](update-ps1-ota-env.md) - (fixed) The network OTA branch of `update.ps1` uploaded from `.pio/build//firmware.bin` because `$brdEnv` was undefined there.
* [Weighted-random off-by-one](weighted-random-off-by-one.md) - (fixed) `random16(total+1)` sometimes selected nothing, silently skipping a scheduled effect switch.

# Risks

* [Static IP config unused](static-ip-config-unused.md) - (fixed) The boards use DHCP, but broadcast self-detection and the gateway ping used `IP_ADDR`/`IP_GW` from `config.h`, and the Dev addresses disagreed.
* [Hard-coded upload token](hardcoded-upload-token.md) - (mitigated) The upload token now lives in the git-ignored `include/secrets.h`; the current value is still in git history until rotated.
* [Effect ID inconsistency](effect-id-case.md) - (fixed) `FxC4` is the only ID that is not uppercase, and ID lookups return 0 on a miss.

# Cleanup

* [Audio remnants](audio-remnants.md) - (fixed) Microphone-era types, enum values and web UI chart code from the RP2040 line, now removed.

# Documentation

* [Documentation drift](documentation-drift.md) - (mitigated) Project documents and script headers now describe the Plasma 2350 W; library metadata and some code comments still describe the Nano RP2040 Connect.
