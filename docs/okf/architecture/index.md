# Architecture

* [Boot sequence](boot-sequence.md) - The order in which CORE0 and CORE1 bring up the filesystem, effects, Wi-Fi, time, comms and diagnostics, and how they hand off with task notifications.
* [Task model](task-model.md) - Every FreeRTOS task: its core, priority, stack size and responsibility.
* [Inter-task messaging](inter-task-messaging.md) - The four FreeRTOS queues, their message enums, and who produces and consumes each one.
* [Software timers](software-timers.md) - Every periodic and one-shot FreeRTOS timer, its period, and the queue message it posts.
* [Memory model](memory-model.md) - The 192 KB heap_4 FreeRTOS heap next to the newlib and lwIP heaps, static LED buffers, and the patterns used to limit fragmentation.
* [Watchdog and health monitoring](watchdog-and-health.md) - How the FX task owns the hardware watchdog, CORE0 starvation detection, OTA mode, and the scratch-register markers and health event file that explain reboots.
* [System status flags](system-status-flags.md) - The `SysStatus` bitmask that gates behavior, and the GPIO-driven on-board RGB status LED colors.
