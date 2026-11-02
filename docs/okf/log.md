# Bundle Update Log

## 2026-10-29 (lwIP options)
* **Changed in code** (working tree, uncommitted): `include/lwipopts.h` was trimmed to the options the project sources see through the lwIP headers (platform hooks and feature switches that shape the lwIP structures); the sizing, TCP and SNTP options were removed. A header comment states that the framework's `include/lwipopts.h` is the master, that `lib/rp2350/liblwip.a` is precompiled, and which settings cannot be changed from the project. `LWIP_ASSERT_CORE_LOCKED` and `LWIP_NETIF_TX_SINGLE_PBUF` were added to match the framework values.
* **Finding**: the firmware runs on the framework's lwIP sizes (16 KB heap, 24 pbufs, 5 TCP and 7 UDP PCBs), read from the `rp2350-rel` ELF symbol sizes. The earlier 32 KB heap and larger pools in the project file never took effect. The macros the project sees match the framework's for every remaining option.
* **Updated concepts**: [framework customizations](/build/framework-customizations.md), [memory model](/architecture/memory-model.md), [Wi-Fi and mDNS](/network/wifi-and-mdns.md), [time sync](/scheduling/time-sync.md), [overview](/overview.md), [documentation drift](/issues/documentation-drift.md). AGENTS.md, README.md and `.github/copilot-instructions.md` were corrected as well.
* **Verified**: `rp2350-rel` builds. Not tested on hardware.

## 2026-10-27 (standard time management)
* **Changed in code** (working tree, uncommitted): the custom NTP client, the drift correction and the hard-coded zone rules in `lib/TimeLib` were replaced by platform primitives. The system clock is set by the lwIP SNTP client (one server, 12 h poll, `SNTP_UPDATE_DELAY` and `-Wl,--wrap=__setSystemTime` in `platformio.ini`); local time uses the POSIX `TZ_America_Chicago` string from the framework's `TZ.h` through newlib. `timeUpdate` is now an hourly housekeeping task. The `/status.json` drift fields were replaced by `syncCount`, `syncRejected`, `lastSync` and `lastSyncAge`, and the web UI shows the last sync.
* **Updated concepts**: [time sync](/scheduling/time-sync.md), [software timers](/architecture/software-timers.md), [memory model](/architecture/memory-model.md), [system status flags](/architecture/system-status-flags.md), [framework customizations](/build/framework-customizations.md), [local libraries](/build/local-libraries.md), [REST API](/network/rest-api.md), [Wi-Fi time fallback](/issues/wifi-gettime-fallback.md).
* **Finding**: the framework's lwIP is precompiled with its own `lwipopts.h`, so SNTP settings in `include/lwipopts.h` do not reach it. (The buffer and pool sizes were checked afterwards - see the 2026-10-29 entry.)
* **Verified**: `rp2350-rel` builds; the compiled SNTP object holds the 12 h interval and its clock-set call goes through the wrap. Not tested on hardware.

## 2026-10-03 (fixes after review)
* **Fixed in code** (working tree after `6d11a87`, uncommitted): [Wi-Fi time fallback](/issues/wifi-gettime-fallback.md), [static IP config unused](/issues/static-ip-config-unused.md) (Dev reservation is 192.168.0.75), [audio remnants](/issues/audio-remnants.md), and the `rp2040_rom_version()` and missing `stringutils.h` errors that broke `LOGGING_ENABLED` builds.
* **Fixed in docs**: AGENTS.md, README.md, `.github/copilot-instructions.md`, `.claude/instructions.md`, script headers and `scripts/synthetic/README.md` now describe the Plasma 2350 W. [Documentation drift](/issues/documentation-drift.md) is now mitigated.
* **Updated concepts**: time sync, Wi-Fi and mDNS, broadcast, REST API, boards, inter-task messaging, effect registry, build and deploy, hardware debugging (the `rp2350-dbg` build currently fails in FastLED's compile test).
* **Verified**: `rp2350-rel` builds for both boards, with and without `LOGGING_ENABLED`, using a placeholder `FW_AUTH_TOKEN`. Not tested on hardware.

## 2026-10-03 (review for RP2350)
* **Review**: Re-checked every concept against branch `dev/plasma2350w` at commit `6d11a87` (Pimoroni Plasma 2350 W). The bundle had been generated from the RP2040 code line and copied here. All source links now point at `6d11a87`.
* **Rewritten** for the RP2350 board: [overview](/overview.md), [boot sequence](/architecture/boot-sequence.md), [task model](/architecture/task-model.md), [inter-task messaging](/architecture/inter-task-messaging.md), [memory model](/architecture/memory-model.md), [software timers](/architecture/software-timers.md), [system status flags](/architecture/system-status-flags.md), [watchdog and health](/architecture/watchdog-and-health.md), [Wi-Fi and mDNS](/network/wifi-and-mdns.md), [REST API](/network/rest-api.md), [multi-board broadcast](/network/multi-board-broadcast.md), [OTA upgrade](/network/ota-firmware-upgrade.md), all [hardware](/hardware/index.md) and [build](/build/index.md) concepts, and [documentation drift](/issues/documentation-drift.md).
* **Updated**: effects, scheduling and persistence concepts (no microphone or audio bumps, coalesced state saves, alarm mutex, 1 MB filesystem, health event file, synthetic seed generator present here).
* **Added issues**: [Wi-Fi time fallback](/issues/wifi-gettime-fallback.md), [static IP config unused](/issues/static-ip-config-unused.md), [audio remnants](/issues/audio-remnants.md).
* **Removed**: `issues/mic-queue-item-size.md`. This board has no microphone task or `micQueue`; the defect only exists on the RP2040 line.
* **Resolved**: [weighted-random off-by-one](/issues/weighted-random-off-by-one.md) is committed here (`6d11a87`).
* **Note**: Still machine-generated by `claude_code/claude-opus-5-5` from source reading (trust tier: unverified). Nothing was confirmed on hardware, and no build was run during this review.

## 2026-10-03
* **Initialization**: Created the bundle from the RP2040 source tree at commit `73c7243` on branch `dev/12-fxe`, including one uncommitted change to `src/EffectRegistry.cpp` (the weighted-random off-by-one fix), then copied it to this branch.
* **Creation**: Wrote concepts for architecture, effects, scheduling, network, hardware, persistence and build, plus an [issues](/issues/index.md) register.
* **Note**: Every concept is machine-generated by `claude_code/claude-opus-5-5` from source reading and has not been reviewed yet (trust tier: unverified). Nothing was confirmed on hardware.
