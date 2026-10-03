---
okf_version: "0.2"
---

# RP2040 LightFX Knowledge Bundle

* [Project overview](overview.md) - What the firmware does, which hardware it runs on, and where to start.

# Subdirectories

* [architecture](architecture/index.md) - Boot sequence, FreeRTOS tasks and cores, queues, timers, memory, watchdog and status flags.
* [effects](effects/index.md) - The effect lifecycle, the registry and weighted selection, the full effect catalog, transitions and how to add an effect.
* [scheduling](scheduling/index.md) - Holidays and palettes, the sleep/wake schedule, time-of-day dimming and NTP time sync.
* [network](network/index.md) - Wi-Fi and mDNS, the REST API, multi-board effect broadcast and OTA firmware upgrade.
* [hardware](hardware/index.md) - Per-board configuration, the controller circuit and the on-board diagnostic sensors.
* [persistence](persistence/index.md) - LittleFS layout and the JSON state files that survive reboots.
* [build](build/index.md) - Build and deploy scripts, build flags, framework customizations, local libraries and hardware debugging.
* [issues](issues/index.md) - Known defects, risks and documentation drift found while building this bundle.