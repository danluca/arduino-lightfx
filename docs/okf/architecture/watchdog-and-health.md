---
type: Architecture
title: Watchdog and health monitoring
description: The FX task feeds the 8-second hardware watchdog through HealthMonitor, which also resets the board when CORE0 starves. Scratch registers and a health event file explain the last reboot.
tags: [watchdog, reliability, diagnostics, reboot]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: hm
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/HealthMonitor.cpp
    title: src/HealthMonitor.cpp
  - id: const
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/constants.hpp
    title: include/constants.hpp (reset markers, scratch indexes)
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/util.cpp
    title: src/util.cpp (watchdogSetup)
  - id: reset
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/reset_markers.cpp
    title: src/reset_markers.cpp
  - id: persist
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/sysinfo_persist.cpp
    title: src/sysinfo_persist.cpp (saveRebootHealthEvent, saveSlownessHealthEvent)
  - id: diag
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/diag.cpp
    title: src/diag.cpp (checkFxHeartbeat)
---

# Watchdog

* `watchdogSetup()` runs late in CORE0 setup. It enables the watchdog with an **8192 ms** timeout, pausing in debug. If the previous reset was caused by the watchdog, it marks the boot as dirty.[^util]
* **Only the FX task feeds it**, through `HealthMonitor::update(7000, 4000)` at the end of every `fx_run()` (also while the OTA upgrade pattern is showing). That call runs `watchdog_update()` only when the board is healthy.[^hm]
* A wedged render loop therefore reboots the board within about 8 s.

# HealthMonitor

* `checkIn(HEALTH_CORE0 | HEALTH_CORE1 | HEALTH_FX)` stores `millis()` in an atomic and in a scratch register. When the watchdog has less than 2000 ms left, it logs "Task(s) slow!" (at most once per second) and writes a slowness [health event](#health-event-file).
* `update(timeoutMs, warnMs)` decides whether to feed the watchdog:
  * **FX** silent for more than `timeoutMs` (7000 ms) → stop feeding.
  * **CORE0** silent for more than **15 s** (**120 s** while an OTA upload is in progress) → write marker `0xA11CE526` and stop feeding. Long CORE0 operations (Wi-Fi connect, pings, client scans, raw uploads) call `checkIn(HEALTH_CORE0)` to stay under this limit.
  * **CORE1** is tracked and logged only, because it normally blocks on its queue.
  * Any gap above `warnMs` (4000 ms) logs "Task(s) slow!". A stall logs "STALLED! STOPS PINGING WATCHDOG" and writes a health event.[^hm]
* `enterOtaMode()` / `exitOtaMode()` are called by the `/fw` upload handler. While OTA mode is on, CORE0 skips `commRun()` and the CORE0 threshold is 120 s.
* `watchdog_get_time_remaining_ms()` is used for the low-watermark warning. The RP2040 line avoids it because of an RP2040 hardware bug; the RP2350 reports it correctly.

# Scratch registers (survive a watchdog reboot)

| Index | Content |
|---|---|
| 7 | Reset marker: why the last reboot happened |
| 6 | FX heartbeat (millis) |
| 5 | FX stage marker (`0xF0000001`–`0xF0000007`: enter, after queue, after OTA check, firmware upgrade, before loop, after loop, after ping) |
| 4 | CORE0 heartbeat |
| 3 | CORE1 heartbeat |
| 2 | FS blocked marker (`0xFB000000` magic) |

# Reset markers (scratch 7)

| Value | Cause | Written by |
|---|---|---|
| `0xA11CE520` | `panic()` | `reset_markers.cpp` (weak override) |
| `0xA11CE521` | `assert` failure | `__assert_func` override |
| `0xA11CE522` | Hard fault | `HardFault_Handler` / `hard_fault_handler` |
| `0xA11CE523` | `pvPortMalloc` failure | `vApplicationMallocFailedHook` |
| `0xA11CE524` | Stack overflow | `vApplicationStackOverflowHook` |
| `0xA11CE525` | FX stall detected (heartbeat unchanged for more than 4 s) | `checkFxHeartbeat` on CORE1 |
| `0xA11CE526` | CORE0 starvation (more than 15 s, or 120 s in OTA mode) | `HealthMonitor::update` |
| `0xA11CE502` | OTA upgrade reboot | `fw_upgrade` |
| `0xA11CE503` | Intentional `reboot()` | `util.cpp` |
| `0xA11CE504` | Unknown | — |

Every fault handler writes its marker and calls `watchdog_reboot(0, 0, 10)`, so faults turn into fast, diagnosable reboots instead of hangs.[^reset][^const]

# Health event file

`/status/health_event.json` holds two sections, served as `GET /health.json`:[^persist]

* `reboot`: written once per boot by `saveRebootHealthEvent()` (end of CORE0 setup) from the reset reason and scratch registers 7, 5 and 2.
* `slowness`: written asynchronously by `saveSlownessHealthEvent()` when the monitor sees slowness or a stall, with the CORE0, CORE1 and FX gaps.

# Diagnosing a reboot

1. `GET /health.json` for the last reboot reason, marker and FX stage.
2. `GET /status.json` → `watchdogRebootsCount`, `lastWatchdogReboot`, `cleanBoot`, `watchdogReboots[]` (each with `reason`, `marker`, `fxStage`, `fsBlocked`).
3. FX stall logs from CORE1 include the FX and CORE1 task states plus a full task stats dump (logging builds only).

[^hm]: src/HealthMonitor.cpp
[^const]: include/constants.hpp (reset markers, scratch indexes)
[^util]: src/util.cpp (watchdogSetup)
[^reset]: src/reset_markers.cpp
[^persist]: src/sysinfo_persist.cpp (saveRebootHealthEvent, saveSlownessHealthEvent)
[^diag]: src/diag.cpp (checkFxHeartbeat)
