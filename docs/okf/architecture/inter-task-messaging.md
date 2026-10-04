---
type: Architecture
title: Inter-task messaging
description: The four FreeRTOS queues, their message types, producers and consumers.
tags: [freertos, queues, messaging, concurrency]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: taskmsg
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/task_msg.h
    title: include/task_msg.h
  - id: taskmsgcpp
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/task_msg.cpp
    title: src/task_msg.cpp (task_msg_setup)
---

# Queues

All queues are created in `task_msg_setup()` and store messages **by value**.[^taskmsgcpp]

| Queue | Depth | Item | Consumer task | Messages |
|---|---|---|---|---|
| `fxQueue` | 10 | `FxActionMessage {FxAction action; uint32_t data;}` | Fx (`fx_run`, non-blocking) | `AUTO_FX`, `MANUAL_FX`, `COLOR_THEME`, `STRIP_BRIGHTNESS`, `SLEEP_ENABLED`, `SLEEP_STATE`, `SAVE_STATE` |
| `almQueue` | 10 | `AlmAction` | ALM (`alarm_misc_run`, blocking) | `ALARM_SETUP`, `ALARM_CHECK`, `SAVE_SYS_INFO`, `SAVE_SYS_INFO_DONE`, `HOLIDAY_UPDATE` |
| `bcQueue` | 20 | `bcTaskMessage {CommAction event; uint16_t data;}` | CORE0 (`commRun`, non-blocking) | `TIME_SETUP`, `TIME_UPDATE`, `FX_SYNC`, `WIFI_ENSURE`, `STATUS_LED_CHECK`, `ENABLE_BROADCAST`, `SCAN_CLIENTS` |
| `diagQueue` | 20 | `DiagAction` | CORE1 (`diagExecute`, blocking) | `RND_ENTROPY`, `SYS_TEMP`, `SYS_VOLTAGE`, `DIAG_INFO`, `RESET_CALIBRATION`, `FX_HEARTBEAT`, `TASK_SNAPSHOT` |

There is no `micQueue` on this board, and the RP2040 line's audio message types were removed. See [audio remnants](/issues/audio-remnants.md).[^taskmsg]

`loop()` skips `commRun()` while an OTA upload is in progress (`HealthMonitor::isInOtaMode()`), so `bcQueue` messages wait until the upload ends or fails.

# Producers

* **HTTP `PUT /fx`** (CORE0) → `fxQueue` (auto, effect, holiday, brightness, sleep), `diagQueue` (reset calibration), `bcQueue` (broadcast). See [REST API](/network/rest-api.md).
* **Software timers** (Tmr Svc) → `bcQueue`, `diagQueue`, `almQueue`. Timer callbacks always send with timeout 0, as FreeRTOS requires. See [software timers](/architecture/software-timers.md).
* **Alarm handlers** `wakeup()` and `bedtime()`, and `adjustCurrentEffect()` (all on ALM) → `fxQueue` `SLEEP_STATE`.
* **FX task** → `bcQueue` `FX_SYNC` (through `postFxChangeEvent` on every effect transition).
* **Comms `ENABLE_BROADCAST`** → `fxQueue` `SAVE_STATE`.
* **CORE0 boot and `timeUpdate`** → `almQueue` `ALARM_SETUP` (through `enqueueAlarmSetup`).

# Task notifications (not queues)

| Notification | From → To | Meaning |
|---|---|---|
| value 1, then 2 | CORE0 → CORE1 | Boot gates: start FX, then Wi-Fi is ready. See [boot sequence](/architecture/boot-sequence.md) |
| `OTA_UPGRADE_NOTIFY` (0xDC) | web handler → CORE0 | Firmware image uploaded and verified |
| `OTA_UPGRADE_NOTIFY` (0xDC) | CORE0 → Fx | Show the upgrade light pattern. See [OTA upgrade](/network/ota-firmware-upgrade.md) |

# Conventions

* To add a message, extend the enum in `task_msg.h`, then add a `case` in the consumer's switch. The `default` branch logs `"... not supported"`.
* Use `getTimerId` and `getTimerName` in timer callbacks. They are null-safe and assume the timer ID points at a `uint16_t`.

[^taskmsg]: include/task_msg.h
[^taskmsgcpp]: src/task_msg.cpp (task_msg_setup)
