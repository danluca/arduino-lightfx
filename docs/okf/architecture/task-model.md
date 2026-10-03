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

| Task | Core | Priority | Stack | Entry points | Responsibility |
|---|---|---|---|---|---|
| CORE0 | 0 | 5 (lowered by 1 at the end of setup) | 2048 B (`configCORE0_TASK_STACK_DEPTH`) | `setup` / `loop` | Web server, comms queue (`bcQueue`) dispatch, OTA trigger |
| ALM | 0 | 5 | 1536 B | `alarm_misc_begin` / `alarm_misc_run` | Alarm setup and checks, saving sys info, holiday updates (`almQueue`) |
| FS | 0 | 7 | (library) | `lib/FilesystemTask` | Serializes all LittleFS access (LittleFS is not thread-safe) |
| CORE1 | 1 | default (same as FX) | 2048 B (`configCORE1_TASK_STACK_DEPTH`) | `setup1` / `loop1` | Diagnostics (`diagQueue`): temperatures, voltage, entropy, FX heartbeat |
| Fx | 1 | 7 | 1536 B | `fx_setup` / `fx_run` | Effect rendering, `fxQueue` dispatch, watchdog ownership |
| Mic | 1 | 5 | 1024 B | `mic_setup` / `mic_run` | PDM microphone peak detection, `micQueue` dispatch |
| SRL | any | 6 | (library) | `lib/PicoLog` | Streams the log ring buffer to serial |
| Tmr Svc, IDLE0/1 | any | kernel | kernel | — | FreeRTOS timer service and idle tasks |

Priorities and stacks come from the `TaskDef` constants in `Main.cpp`. `configMAX_PRIORITIES=12`.[^main][^pio] The comment block at the top of `Main.cpp` lists slightly different priorities (FX 6, Mic 6, ALM "inherited 5"). Treat the `TaskDef` values as authoritative.

# Rules that come from experience

* **Wi-Fi stays on CORE0.** The WiFiNINA driver only works reliably from CORE0. The on-board RGB LED is wired to the NINA module, so `updateBoardLED` must also run on the Wi-Fi task. Calling it from another task has caused lockups and resets.[^main]
* **FX and Mic must share a core.** The code notes an unexplained coupling: they work together on either core, but not on different cores.[^main]
* **Web server and comms run in the same task on purpose.** `web_run()` calls `webserver()` and then `commRun()` one after the other. Because only one task ever touches `fxBroadcastRecipients`, it needs no locks.[^main]
* **CORE1 and FX share a priority on purpose.** They round-robin, so neither starves the other. Because FX owns the [watchdog](/architecture/watchdog-and-health.md), starving FX would reboot the board.
* **Each long-lived worker blocks on its own queue.** ALM and CORE1 block with `portMAX_DELAY`. FX, Mic and CORE0 poll with timeout 0 inside their loops.
* **Release builds disable the USB task** (`PIO_FRAMEWORK_ARDUINO_NO_USB` is added by `scripts/util.ps1` when neither `-log` nor `-dbg` is given).

# Related

* [Inter-task messaging](/architecture/inter-task-messaging.md)
* [Cross-task effect registry access](/issues/cross-task-registry-access.md): a known exception to the "talk through queues" rule.

[^main]: src/Main.cpp (TASK ALLOCATIONS comment, TaskDef constants)
[^pio]: platformio.ini
