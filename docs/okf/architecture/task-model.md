---
type: Architecture
title: Task model
description: The FreeRTOS tasks across both RP2350 cores, with their priorities, stack sizes and responsibilities.
tags: [freertos, tasks, cores, concurrency]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: main
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/Main.cpp
    title: src/Main.cpp (TASK ALLOCATIONS comment, TaskDef constants)
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/platformio.ini
    title: platformio.ini
  - id: fs
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/lib/FilesystemTask/src/filesystem.cpp
    title: lib/FilesystemTask/src/filesystem.cpp (fsDef)
  - id: log
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/lib/PicoLog/src/PicoLog.cpp
    title: lib/PicoLog/src/PicoLog.cpp (tdStream)
  - id: sched
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/lib/SchedulerExt/src/SchedulerExt.cpp
    title: lib/SchedulerExt/src/SchedulerExt.cpp (TaskWrapper::run)
---

# Tasks

Stack sizes are FreeRTOS stack depths, counted in **words** (`StackType_t` is `uint32_t` on this port), so 1536 means 6 KB.

| Task | Core | Priority | Stack (words) | Entry points | Responsibility |
|---|---|---|---|---|---|
| CORE0 | 0 | 6, lowered to 5 at the end of setup | 3072 (`configCORE0_TASK_STACK_DEPTH`) | `setup` / `loop` | Web server, comms queue (`bcQueue`) dispatch, OTA trigger |
| ALM | 0 | 5 | 1536 | `alarm_misc_begin` / `alarm_misc_run` | Alarm setup and checks, saving sys info, holiday updates (`almQueue`) |
| FS | 0 | 7 (caller's priority + 1, created while CORE0 is still at 6) | 1536 | `lib/FilesystemTask` | Serializes all LittleFS access (LittleFS is not thread-safe) |
| CORE1 | 1 | 6 | 3072 (`configCORE1_TASK_STACK_DEPTH`) | `setup1` / `loop1` | Diagnostics (`diagQueue`): CPU temperature, voltage, entropy, FX heartbeat, task snapshots |
| Fx | 1 | 7 | 1536 | `fx_setup` / `fx_run` | Effect rendering, `fxQueue` dispatch, watchdog ownership |
| SRL | any | 4 | 1024 | `lib/PicoLog` | Streams the log ring buffer to serial. Below all app tasks, so it drains during their idle gaps |
| Tmr Svc, IDLE0/1, IdleCore0/1 | any | kernel | kernel | — | FreeRTOS timer service and idle tasks |

The framework creates the CORE0 and CORE1 tasks at `configMAX_PRIORITIES / 2` = 6. ALM and Fx priorities come from the `TaskDef` constants in `Main.cpp`; `configMAX_PRIORITIES=12`.[^main][^pio][^fs][^log]

There is **no Mic task**: the Plasma 2350 W has no microphone.

# Rules that come from experience

* **Network work stays on CORE0.** The web server, comms, NTP, mDNS and pings all run on the CORE0 task, so network state needs no locks. The `Main.cpp` header still cites the WiFiNINA driver as the reason; that comment comes from the RP2040 code line.
* **Web server and comms run in the same task on purpose.** `loop()` calls `webserver()` and then `commRun()` one after the other. Because only one task ever touches `fxBroadcastRecipients`, it needs no locks.[^main]
* **FX outranks CORE1 but yields.** FX (7) is above CORE1 (6) on core 1. `SchedulerExt` calls `vTaskDelay(1)` after every `fx_run()`, which gives CORE1 its time slot.[^sched] The `setup1()` comment that says the two share a priority and round-robin is out of date.
* **Each long-lived worker blocks on its own queue.** ALM and CORE1 block with `portMAX_DELAY`. FX and CORE0 poll with timeout 0 inside their loops.
* **CORE0 must keep checking in.** Long blocking work on CORE0 (Wi-Fi connect, gateway pings, client scans, raw uploads) calls `HealthMonitor::checkIn(HEALTH_CORE0)`. More than 15 s without a check-in (120 s during an OTA upload) stops the watchdog feed. See [watchdog and health](/architecture/watchdog-and-health.md).
* **Release builds disable the USB task** (`PIO_FRAMEWORK_ARDUINO_NO_USB` is added by `scripts/util.ps1` when neither `-log` nor `-dbg` is given).

# Related

* [Inter-task messaging](/architecture/inter-task-messaging.md)
* [Cross-task effect registry access](/issues/cross-task-registry-access.md): a past exception to the "talk through queues" rule, now fixed.

[^main]: src/Main.cpp (TASK ALLOCATIONS comment, TaskDef constants)
[^pio]: platformio.ini
[^fs]: lib/FilesystemTask/src/filesystem.cpp (fsDef)
[^log]: lib/PicoLog/src/PicoLog.cpp (tdStream)
[^sched]: lib/SchedulerExt/src/SchedulerExt.cpp (TaskWrapper::run)
