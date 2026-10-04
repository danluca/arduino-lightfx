---
type: Issue
title: Documentation drift
description: README.md, AGENTS.md, the assistant instruction files, script headers, library metadata and several code comments described the Nano RP2040 Connect code line instead of this Plasma 2350 W code line at commit 6d11a87. The project documents are fixed; library metadata and some code comments remain.
tags: [documentation, drift, readme, agents, rp2040]
severity: low
issue_state: mitigated
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: readme
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/README.md
    title: README.md
    author: human:danluca
  - id: agents
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/AGENTS.md
    title: AGENTS.md
    author: human:danluca
  - id: copilot
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/.github/copilot-instructions.md
    title: .github/copilot-instructions.md
  - id: claudeinst
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/.claude/instructions.md
    title: .claude/instructions.md
    author: human:danluca
---

# Background

Commit `6d11a87` added this bundle and "aligned documentation with code". The bundle and the project documents were generated from the RP2040 code line (`dev/12-fxe`) and copied here, so they described a different board. The bundle has since been reviewed against this branch.

# Resolution (working tree after `6d11a87`)

**Fixed**: AGENTS.md, README.md, `.github/copilot-instructions.md`, `.claude/instructions.md`, the `update.ps1`/`ota_upgrade.ps1`/`seriallog.ps1` headers, `scripts/synthetic/README.md`, the `config.h` `IP_ADDR` comments, the `handleNTPFailure`/`timeSetup`/`setWiFiInfo` comments, and `logSystemInfo()` (it called `rp2040_rom_version()`, which broke every `LOGGING_ENABLED` build; it now calls `rp2350_rom_version()`).

**Still open**: the `library.json` metadata, `include/lwipopts.h`, `docs/debugging.md` and the remaining code comments in the second table. The tables below record the original findings.

# Project docs

| Where | Says | This branch |
|---|---|---|
| AGENTS.md | Board is the Arduino Nano RP2040 Connect: dual Cortex-M0+ at 133 MHz, 264 KB RAM, 144 KB heap, 16 MB flash; PDM mic, LSM6DSOX IMU, ECC608; envs `rp2040-rel`/`rp2040-dbg`; WiFiNINA port and a local mDNS library; NINA-W102 Wi-Fi | Pimoroni Plasma 2350 W: RP2350 Cortex-M33 at 150 MHz, 520 KB SRAM, 192 KB heap, 4 MB firmware + 1 MB LittleFS; no mic, IMU or ECC608; envs `rp2350-rel`/`rp2350-dbg`; framework `WiFi` + lwIP and `LEAmDNS`; CYW43439 Wi-Fi[^agents] |
| README.md | Title "RP2040 LightFX"; Nano RP2040 Connect as primary board; 144 KB heap; "Audio Reactive" PDM microphone; WiFiNINA; GPIO 25 / D2; `pio run -e rp2040-rel`; 7805 + 74HCT125 circuit; PRs against `dev/12-fxe` | See [overview](/overview.md), [boards](/hardware/boards-and-config.md) and [controller board](/hardware/controller-circuit.md)[^readme] |
| `.github/copilot-instructions.md` | "rp2040-lightfx"; envs `rp2040-rel`/`rp2040-dbg`; 144 KB heap; WiFiNINA; heap logging in `src/sysinfo.cpp` | `rp2350-*` envs; 192 KB heap; heap stats in `src/sysinfo_runtime.cpp`[^copilot] |
| `.claude/instructions.md` | Title "RP2040 LightFX"; "only 512KB heap available"; branches `dev/12-fxe` and `rel/nanorp2040connect` | Body correctly names the Plasma 2350 W; the heap is 192 KB of 520 KB SRAM; this line develops on `dev/plasma2350w`[^claudeinst] |
| `update.ps1`, `ota_upgrade.ps1`, `seriallog.ps1` headers | "for Arduino Nano RP2040 Connect boards" | They target the Plasma 2350 W (`rp2350:rp2350:pimoroni_plasma2350`, `rp2350-rel`) |
| `scripts/synthetic/README.md` | The generator and its tests "live in the RP2350 sibling project" and are not in this repository | `scripts/create_synthetic_seed.py` and `scripts/test_synthetic_seed.py` are here. See [FXI4 audio seeds](/effects/fxi4-audio-seeds.md) |
| `docs/debugging.md`, `docs/Controller_*` | Nano RP2040 Connect debugging and circuit | Not applicable as written. See [hardware debugging](/build/hardware-debugging.md) and [controller board](/hardware/controller-circuit.md) |
| `lib/RestWebServer/library.json` | Derived "to work with WiFiNINA and avoid pulling in the lwip layer" | Runs on the framework `WiFi`/lwIP classes |
| `lib/TimeLib/library.json` | "not yet updated for RP2350" | Used on the RP2350 |
| `include/lwipopts.h` | `MEM_SIZE` comment: "for RP2350's 256KB heap" | 192 KB FreeRTOS heap; 520 KB SRAM |

# Code comments and identifiers

| Where | Comment or identifier | Behavior |
|---|---|---|
| `Main.cpp` header | Mic task, FX/Mic same-core coupling, WiFiNINA only working on CORE0, CORE tasks with 1024-byte stacks | No Mic task; CORE stacks are 3072 words. See [task model](/architecture/task-model.md) |
| `Main.cpp` `setup1` | CORE1 and FX share a priority and round-robin | CORE1 is 6, FX is 7; FX yields with `vTaskDelay(1)` |
| `Main.cpp` `setup`, `setup1` | "Manual updates to the pico framework changed the stack size to 2048 bytes" | 3072 words through `configCORE*_TASK_STACK_DEPTH` and the fork |
| `util.cpp` `secRandom8/16` | "Leverages ECC608B's … Random Number Generator … takes about 30 ms" | Uses the RP2350 hardware RNG (`rp2040.hwrand32()`); fast |
| `diag.cpp` `updateSecEntropy`, `diagSetup` | "from the ECC608 security chip"; "both the IMU chip and the CPU" temperatures | Hardware RNG; CPU temperature only |
| `diag.cpp` `chipTemperature` | RP2040 datasheet formula and page reference | Fallback only; the RP2350 sensor has its own datasheet values |
| `net_setup.cpp` `wifi_setup` | "enable low-power mode" | Calls `WiFi.noLowPowerMode()` |
| `timeutil.cpp` `handleNTPFailure` doc | "Wi-Fi time, potentially sourced from NTP" (fixed together with the code) | See [Wi-Fi time fallback](/issues/wifi-gettime-fallback.md) |
| `constants.hpp`, `web_server.cpp` | `kUaBoardPrefix = "rp2040-lightfx-master"`, `serverAgent = "rp2040-luca/1.0.0"` | Wire identifiers; renaming would only matter if a follower checks them (none does today) |
| `config.h` | `IP_ADDR` per board as a "static IP" (fixed) | DHCP with router reservations. See [static IP config unused](/issues/static-ip-config-unused.md) |

# Suggested handling

Use this bundle as the reconciled reference. Update AGENTS.md first (it is loaded into every agent session), then README.md, `.github/copilot-instructions.md` and `.claude/instructions.md`, then the script headers and `scripts/synthetic/README.md`. Code comments can follow with the next change to each file.

[^readme]: README.md
[^agents]: AGENTS.md
[^copilot]: .github/copilot-instructions.md
[^claudeinst]: .claude/instructions.md
