# Architecture

* [Boot sequence](boot-sequence.md) - The order in which CORE0 and CORE1 bring up the filesystem, effects, Wi-Fi, time, comms and diagnostics, and how they hand off with task notifications.
* [Task model](task-model.md) - Every FreeRTOS task: its core, priority, stack size and responsibility.
* [Inter-task messaging](inter-task-messaging.md) - The five FreeRTOS queues, their message enums, and who produces and consumes each one.
* [Software timers](software-timers.md) - Every periodic and one-shot FreeRTOS timer, its period, and the queue message it posts.
* [Memory model](memory-model.md) - The heap_4 FreeRTOS heap, static LED buffers, and the patterns used to limit fragmentation.
* [Watchdog and health monitoring](watchdog-and-health.md) - How the FX task owns the hardware watchdog, and the scratch-register markers that explain reboots.
* [System status flags](system-status-flags.md) - The `SysStatus` bitmask that gates behavior, and the on-board RGB status LED colors.
