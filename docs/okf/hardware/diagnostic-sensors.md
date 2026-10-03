---
type: Architecture
title: Diagnostic sensors
description: What the CORE1 diagnostics task measures (IMU and self-calibrated CPU temperatures, supply voltage, secure random entropy), plus the NINA chip temperature read on CORE0 and the PDM microphone that triggers effect bumps.
tags: [diagnostics, sensors, temperature, calibration, microphone, ecc608]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: diag
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/diag.cpp
    title: src/diag.cpp
  - id: mic
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/mic.cpp
    title: src/mic.cpp
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/util.cpp
    title: src/util.cpp (secure element)
---

# Temperatures

* **Board:** read from the LSM6DSOX IMU, which is accurate. If the IMU fails to start, the diag setup **suspends CORE1**.
* **CPU:** the RP2040 ADC channel-4 sensor, which is imprecise and **self-calibrated**. Each `SYS_TEMP` (every 32 s) pairs the raw ADC value with the IMU reading. Once the min and max readings are far enough apart, `calibrate()` derives `refTemp`, `vtref` (mV), `slope` (mV/°C) and `refDelta`, and saves them to `/status/calibration.json`. A CPU reading only counts if calibration is valid **and** it is within 15 °C of the IMU. Reset with `PUT /fx {"resetTempCal":true}`.[^diag]
* **Wi-Fi chip:** `WiFi.getTemperature()` every 33 s on CORE0. It discards the bogus 128 °F (53.33 °C) value unless earlier readings were already near it.

# Supply voltage

`SYS_VOLTAGE` every 34 s: A0 through the board-specific divider (`VCC_DIV_R4/R5`) against the measured 3V3 rail. Reported as `vcc` in `/status.json`.

# Secure element and entropy

* `secElement_setup()` on CORE0 during boot. If the ECC608 config is unlocked, it writes `ECCX08_DEFAULT_TLS_CONFIG` and **locks it permanently**, a one-time irreversible step on a new board. It records the serial and sets `SysStatus::Ecc`.[^util]
* `secRandom16()` takes about 30 ms per call. Every 6 min (`RND_ENTROPY`) its output goes into `random16_add_entropy`. Microphone peaks and the saved `randomSeed` add entropy too.

# Microphone (Mic task, CORE1)

* PDM: mono, 24 kHz, 512-sample buffer, gain 5. The ISR callback `onPDMdata` copies into `sampleBuffer`. `mic_run` snapshots it with interrupts off.[^mic]
* When the batch peak exceeds `audioBumpThreshold` (default 5000), it sets `fxBump = true`, adds entropy, and counts the peak in a 10-bin histogram (500-wide bins above the threshold, exposed as `fx.audioHist`).
* The FX task checks `fxBump` every 30 s and moves to the **next** effect. `totalAudioBumps` counts these.
* Updating the threshold through the web is currently broken. See [mic queue item size](/issues/mic-queue-item-size.md).

# Logging

With `LOGGING_ENABLED`, `DIAG_INFO` logs a task summary every 30.25 s through `lib/PicoLog`. PicoLog uses a ring buffer and a dedicated SRL task, so logging rarely blocks the caller. The debug env sets `LOGGING_ENABLED=1`. Release builds turn it on with `build.ps1 -log`.

[^diag]: src/diag.cpp
[^mic]: src/mic.cpp
[^util]: src/util.cpp (secure element)
