---
type: Process
title: Boot sequence
description: Startup order across both cores. CORE0 brings up core services and notifies CORE1 twice, first to start effects and then once Wi-Fi is up.
tags: [boot, setup, freertos, cores]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: main
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/Main.cpp
    title: src/Main.cpp (setup, setup1)
  - id: efx
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/efx_setup.cpp
    title: src/efx_setup.cpp (fx_setup)
---

# CORE0: `setup()`

1. `taskDelay(2000)`: a safety delay.
2. Set up the status LED, logging, and the double-reset bootloader (`RP2040::enableDoubleResetBootloader()`).
3. `sysInfo = new SysInfo()`, then `filesystem_setup()` (starts `SyncFsImpl` over LittleFS and sets `SysStatus::Filesystem`), then `sysInfo->begin()`.
4. `task_msg_setup()` creates all five [queues](/architecture/inter-task-messaging.md).
5. Starts the **ALM** task. Its setup (`state_led_begin`) blinks the status LED green until both `Setup0` and `Setup1` are set, so ALM does not process queue messages until boot finishes.
6. `readSysInfo()` (restores `/sys.json`) and `secElement_setup()` (ECC608 secure element; locks its config if it is unlocked, then seeds entropy).
7. **Notification 1 → CORE1**: `xTaskNotify(CORE1, 1)`, which allows effects to start.
8. `wifi_setup()` blocks until Wi-Fi connects. It retries forever with 30-second attempts. Then `taskDelay(2500)`.
9. `timeSetup()` (NTP, or Wi-Fi time as a fallback), `commSetup()` (comms [timers](/architecture/software-timers.md), broadcast clients, then a 5-second delay), and `web::server_setup()`.
10. **Notification 2 → CORE1**: Wi-Fi is ready.
11. `watchdogSetup()` arms an 8.192-second hardware watchdog. `HealthMonitor::init()` starts health tracking.
12. Lowers its own priority by 1. If NTP succeeded, it enqueues `ALARM_SETUP`.
13. Sets `SysStatus::Setup0` and logs system info.[^main]

`loop()` then runs: health check-in → `web::webserver()` → `commRun()` → `handle_fw_upgrade()` → `vTaskDelay(5)`.

# CORE1: `setup1()`

1. Blocks on `ulTaskNotifyTake` until notification 1.
2. Starts the **FX** task (`fx_setup`/`fx_run`), then `taskDelay(250)`.
3. Blocks until notification 2 (Wi-Fi ready).
4. Starts the **Mic** task, then runs `diagSetup()` (diagnostic timers, ADC, IMU, board ID, temperature calibration).
5. Sets `SysStatus::Setup1`.[^main]

`loop1()` then runs: health check-in → `diagExecute()`, which blocks on `diagQueue` → `vTaskDelay(7)`.

# FX task setup: `fx_setup()`

1. `ledStripInit()`: `addLeds<WS2811, 25, BRG>`, color correction `TypicalSMD5050`, temperature `Tungsten100W`, then clears the strip.
2. Calls each category's `fxRegister()` in the order A, B, C, D, E, F, H, I, J, K. This order fixes the registry index of every effect.
3. `readFxState()` restores `/state.json`: auto-roll, random seed, current effect, brightness, audio threshold, holiday, sleep and broadcast.
4. Shuffles the strip index table and calls `transitionEffect()` then `loop()` to create the first effect.
5. Writes `/status/fxconfig.json` (the holiday list plus the effect list) for the web UI.[^efx]

# Design notes

* FX starts **before** Wi-Fi. The strip lights up even when the network is down. Holiday auto-detection returns the saved holiday until `SysStatus::Wifi` is set.
* Before NTP sync, `resetGlobals()` does not flush the cleared strip (`FastLED.clear(flushStrip)` with `flushStrip = Ntp && !asleep`). This avoids a blink during early boot.
* If the Wi-Fi module is missing (`WL_NO_MODULE`), `wifi_setup` suspends CORE0 indefinitely.

[^main]: src/Main.cpp (setup, setup1)
[^efx]: src/efx_setup.cpp (fx_setup)
