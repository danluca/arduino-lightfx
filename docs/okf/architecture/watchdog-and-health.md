---
type: Architecture
title: Watchdog and health monitoring
description: The FX task feeds the 8-second hardware watchdog through HealthMonitor. Scratch registers record a heartbeat, the FX stage and a reset-cause marker that survive a reboot.
tags: [watchdog, reliability, diagnostics, reboot]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: hm
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/HealthMonitor.cpp
    title: src/HealthMonitor.cpp
  - id: const
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/include/constants.hpp
    title: include/constants.hpp (reset markers, scratch indexes)
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/util.cpp
    title: src/util.cpp (watchdogSetup)
  - id: reset
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/reset_markers.cpp
    title: src/reset_markers.cpp
---

# Watchdog

* `watchdogSetup()` runs late in CORE0 setup. It enables the watchdog with an **8192 ms** timeout, pausing in debug. If the previous reset was caused by the watchdog, it records the time in `SysInfo` (the last 10 are kept) and marks the boot as dirty.[^util]
* **Only the FX task feeds it**, through `HealthMonitor::update(7000, 3000)` at the end of every `fx_run()`. That call runs `watchdog_update()` only if FX checked in within the last 7000 ms. CORE0 and CORE1 check-ins are tracked and logged, but they are not required, because both legitimately block for long periods (Wi-Fi reconnects, pings, queue waits).[^hm]
* A wedged render loop therefore reboots the board within about 8 s. A slow core only produces warnings.

# HealthMonitor

* `checkIn(HEALTH_CORE0 | HEALTH_CORE1 | HEALTH_FX)` stores `millis()` in an atomic and in a scratch register. It logs "Task(s) slow!" at most once per second when any gap exceeds 2000 ms.
* `update(timeoutMs, warnMs)` feeds the watchdog when healthy. When FX has been silent for longer than `timeoutMs`, it logs "STALLED! STOPS PINGING WATCHDOG".
* `watchdog_get_time_remaining_ms()` has a hardware bug on the RP2040 (it always returns 5), so the code does not use it to make decisions.[^hm]

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
| `0xA11CE525` | FX stall detected (more than 2.5 s without a heartbeat) | `checkFxHeartbeat` on CORE1 |
| `0xA11CE502` | OTA upgrade reboot | `fw_upgrade` |
| `0xA11CE503` | Intentional `reboot()` | `util.cpp` |
| `0xA11CE504` | Unknown | — |

Every fault handler writes its marker and calls `watchdog_reboot(0, 0, 10)`, so faults turn into fast, diagnosable reboots instead of hangs.[^reset][^const]

# Diagnosing a reboot

1. `GET /status.json` → `watchdogRebootsCount`, `lastWatchdogReboot`, `cleanBoot`.
2. Read scratch 7 and 5 early in boot (or from logs) to find the cause and the last FX stage reached.
3. FX stall logs from CORE1 include the FX and CORE1 task states plus a full task stats dump.

[^hm]: src/HealthMonitor.cpp
[^const]: include/constants.hpp (reset markers, scratch indexes)
[^util]: src/util.cpp (watchdogSetup)
[^reset]: src/reset_markers.cpp
