---
type: Architecture
title: Task model
description: The FreeRTOS tasks across both RP2040 cores, with their priorities, stack sizes and responsibilities.
tags: [freertos, tasks, cores, concurrency]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: main
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/Main.cpp
    title: src/Main.cpp (TASK ALLOCATIONS comment, TaskDef constants)
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/platformio.ini
    title: platformio.ini
---

# Tasks

Stack sizes are FreeRTOS stack depths, counted in **words** (`StackType_t` is `uint32_t` on this port), so 1536 means 6 KiB.
In the priority expressions below, `max` means `configMAX_PRIORITIES`.

| Task | Core | Priority (12 / 32 levels) | Stack (words) | Entry points | Responsibility |
|---|---|---|---|---|---|
| CORE0 | 0 | `max/2` = 6 / 16; lowered to `max/2 - 1` = 5 / 15 after setup | 2048 (`configCORE0_TASK_STACK_DEPTH`) | `setup` / `loop` | Web server, comms queue (`bcQueue`) dispatch, OTA trigger |
| ALM | 0 | `max/2 - 1` = 5 / 15 | 1536 | `alarm_misc_begin` / `alarm_misc_run` | Alarm setup and checks, saving sys info, holiday updates (`almQueue`) |
| FS | 0 | Caller + 1; normally `max/2 + 1` = 7 / 17 | 1536 | `lib/FilesystemTask` | Serializes all LittleFS access (LittleFS is not thread-safe) |
| CORE1 | 1 | `max/2` = 6 / 16 | 2048 (`configCORE1_TASK_STACK_DEPTH`) | `setup1` / `loop1` | Diagnostics (`diagQueue`): CPU temperature, voltage, entropy, FX heartbeat, task snapshots |
| FX | 1 | `max/2 + 1` = 7 / 17 | 1536 | `fx_setup` / `fx_run` | Effect rendering, `fxQueue` dispatch, watchdog ownership |
| Mic | 1 | `max/2 - 1` = 5 / 15 | 1024 | `mic_setup` / `mic_run` | PDM microphone peak detection, `micQueue` dispatch |
| SRL | any | `max/2 - 2` = 4 / 14 | 1024 | `lib/PicoLog` | Streams the log ring buffer to serial, below the application workers |
| Tmr Svc | any | `configTIMER_TASK_PRIORITY` = 2 / 2 | framework | FreeRTOS | Software timer callbacks |
| IDLE0/1 | each core | 0 / 0 | framework | FreeRTOS | Kernel idle tasks |
| IdleCore0/1 | each core | `max - 1` = 11 / 31 | framework | Framework | Coordinate flash operations on the other core |
| LWIP (conditional) | 0 | `max - 2` = 10 / 30 | framework | Framework | Created only when the framework starts its lwIP worker; the Nano's WiFiNINA path uses the NINA module instead |
| USB | 0 | `max - 2` = 10 / 30 | framework | Framework | USB service task; omitted in release builds |

Both PlatformIO environments set `configMAX_PRIORITIES=32`. The application formulas are relative to the framework midpoint, and retain the prior ordering when built at 12 levels. The FS task remains one above its caller and rejects a caller priority that would produce the scheduler's inherit-caller sentinel.[^main][^pio][^fs][^log]

The locally installed framework CORE0/CORE1 use `max/2`, flash-coordination tasks use `max-1`, LWIP and USB each use `max-2`, and the timer service uses priority 2. The announced upcoming scheduler revision is not available at this commit, so its task priorities remain unverified. The extra ready-list storage and timing effects of 32 levels require measurement; they are not assumed to be a no-op.


# Rules that come from experience

* **Wi-Fi stays on CORE0.** The WiFiNINA driver only works reliably from CORE0. The on-board RGB LED is wired to the NINA module, so `updateBoardLED` must also run on the Wi-Fi task. Calling it from another task has caused lockups and resets.[^main]
* **FX and Mic must share a core.** The code notes an unexplained coupling: they work together on either core, but not on different cores.[^main]
* **Web server and comms run in the same task on purpose.** `loop()` calls `web_run()`, which calls `web::webserver()` and then `commRun()` one after the other. Because only one task ever touches `fxBroadcastRecipients`, it needs no locks.[^main]
* **FX is one priority above CORE1 and blocks for one tick per iteration.** `SchedulerExt` calls `vTaskDelay(1)` after each `fx_run()`. CORE1 may run while FX is blocked, but these tasks do not round-robin at equal priority, and the delay alone does not guarantee that the lower-priority Mic task gets enough CPU time to process PDM samples.[^sched]
* **Mic shares CORE1 with FX and diagnostics.** The Nano RP2040 Connect's PDM callback fills a sample buffer while `mic_run()` processes it; Mic has priority `max/2 - 1`, below both FX and CORE1. Validate microphone processing under real effect and diagnostics load on the board rather than assuming a scheduler delay ensures its sampling workload keeps up.[^main][^mic]
* **Each worker's queue behavior differs.** ALM blocks on `almQueue` and CORE1 blocks on `diagQueue` with `portMAX_DELAY`; FX, Mic, and CORE0 poll their queues with timeout 0 inside their loops.[^diag]
* **Release builds disable the USB task** (`PIO_FRAMEWORK_ARDUINO_NO_USB` is added by `scripts/util.ps1` when neither `-log` nor `-dbg` is given).

# Related

* [Inter-task messaging](/architecture/inter-task-messaging.md)
* [Cross-task effect registry access](/issues/cross-task-registry-access.md): a known exception to the "talk through queues" rule.

[^main]: src/Main.cpp (TASK ALLOCATIONS comment, TaskDef constants)
[^pio]: platformio.ini
[^fs]: lib/FilesystemTask/src/filesystem.cpp (caller-relative filesystem task priority)
[^log]: lib/PicoLog/src/PicoLog.cpp (serial logger task definition)
[^sched]: lib/SchedulerExt/src/SchedulerExt.cpp (`TaskWrapper::run` delays one tick after each loop call)
[^mic]: src/mic.cpp (PDM callback and sample processing)
[^diag]: src/diag.cpp (`diagExecute` blocks on `diagQueue`)
