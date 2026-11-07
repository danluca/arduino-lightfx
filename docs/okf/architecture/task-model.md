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

Stack sizes are FreeRTOS stack depths, counted in **words** (`StackType_t` is `uint32_t` on this port), so 1536 means 6 KiB.
In the priority expressions below, `max` means `configMAX_PRIORITIES`.

| Task | Core | Priority (12 / 32 levels) | Stack (words) | Entry points | Responsibility |
|---|---|---|---|---|---|
| CORE0 | 0 | `max/2` = 6 / 16; lowered to `max/2 - 1` = 5 / 15 after setup | 3072 (`configCORE0_TASK_STACK_DEPTH`) | `setup` / `loop` | Web server, comms queue (`bcQueue`) dispatch, OTA trigger |
| ALM | 0 | `max/2 - 1` = 5 / 15 | 1536 | `alarm_misc_begin` / `alarm_misc_run` | Alarm setup and checks, saving sys info, holiday updates (`almQueue`) |
| FS | 0 | Caller + 1; normally `max/2 + 1` = 7 / 17 | 1536 | `lib/FilesystemTask` | Serializes all LittleFS access (LittleFS is not thread-safe) |
| CORE1 | 1 | `max/2` = 6 / 16 | 3072 (`configCORE1_TASK_STACK_DEPTH`) | `setup1` / `loop1` | Diagnostics (`diagQueue`): CPU temperature, voltage, entropy, FX heartbeat, task snapshots |
| FX | 1 | `max/2 + 1` = 7 / 17 | 1536 | `fx_setup` / `fx_run` | Effect rendering, `fxQueue` dispatch, watchdog ownership |
| SRL | any | `max/2 - 2` = 4 / 14 | 1024 | `lib/PicoLog` | Streams the log ring buffer to serial, below the application workers |
| Tmr Svc | any | `configTIMER_TASK_PRIORITY` = 2 / 2 | framework | FreeRTOS | Software timer callbacks |
| IDLE0/1 | each core | 0 / 0 | framework | FreeRTOS | Kernel idle tasks |
| IdleCore0/1 | each core | `max - 1` = 11 / 31 | framework | Framework | Coordinate flash operations on the other core |
| LWIP | 0 | `max - 2` = 10 / 30 | framework | Framework | Serialize lwIP operations |
| USB | 0 | `max - 2` = 10 / 30 | framework | Framework | USB service task; omitted in release builds |

Both PlatformIO environments set `configMAX_PRIORITIES=32`. The application formulas are relative to the framework midpoint, and retain the prior ordering when built at 12 levels. The FS task remains one above its caller and rejects a caller priority that would produce the scheduler's inherit-caller sentinel.[^main][^pio][^fs][^log]

The locally installed framework and the upstream `feat/stable` branch both resolve to `40409287c9988bfc18e86d4b324431386a99a33e`. In that revision, CORE0/CORE1 use `max/2`, flash-coordination tasks use `max-1`, LWIP and USB each use `max-2`, and the timer service uses priority 2. The announced upcoming scheduler revision is not available at this commit, so its task priorities remain unverified. The extra ready-list storage and timing effects of 32 levels require measurement; they are not assumed to be a no-op.

There is **no Mic task**: the Plasma 2350 W has no microphone.

# Rules that come from experience

* **Network work stays on CORE0.** The web server, comms, NTP, mDNS and pings all run on the CORE0 task, so network state needs no locks; lwIP operations are serialized by the framework's CORE0-affined task.
* **Web server and comms run in the same task on purpose.** `loop()` calls `webserver()` and then `commRun()` one after the other. Because only one task ever touches `fxBroadcastRecipients`, it needs no locks.[^main]
* **FX outranks CORE1 but yields.** FX is one level above CORE1 on core 1. `SchedulerExt` calls `vTaskDelay(1)` after every `fx_run()`, allowing lower-priority CORE1 to run while FX is delayed.[^sched]
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
