---
type: Architecture
title: Diagnostic sensors
description: What the CORE1 diagnostics task measures on the Plasma 2350 W. It reads the RP2350 internal temperature sensor (with stored calibration) and the A0 supply voltage, and seeds FastLED's random generator from the hardware RNG.
tags: [diagnostics, sensors, temperature, calibration, entropy]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: diag
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/diag.cpp
    title: src/diag.cpp
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/util.cpp
    title: src/util.cpp (secRandom)
---

The Plasma 2350 W has **no IMU, no ECC608 secure element and no microphone**. The RP2040 line's board temperature, Wi-Fi chip temperature, secure-element setup and audio bumps do not exist here. Diagnostics run on CORE1 from `diagQueue` messages.

# CPU temperature

* The RP2350 ADC channel 4 sensor, averaged over 8 reads, every 32 s (`SYS_TEMP`).[^diag]
* With valid calibration the reading is `refTemp - (V - vtref) / slope`. Without it, the code falls back to the RP2040 datasheet formula `27 - (V - 706 mV) / 1.721`.
* There is no reference sensor, so calibration cannot be derived the way the RP2040 line does with its IMU. On first boot, `readCalibrationInfo()` writes a **hard-coded reference point** (23.33 °C at ADC 746, `vtref` 598 mV, slope 1.721 mV/°C, measured manually on 2025-11-01) to `/status/calibration.json`.
* `calibrate()` still runs after every reading. It recomputes the parameters once the recorded min and max are far enough apart (5 °C for a first calibration, refDelta + 5 °C afterwards). Because the inputs are CPU readings, this re-derives the calibration from itself; treat CPU temperatures as rough.
* `PUT /fx {"resetTempCal":true}` deletes the calibration file and clears the parameters. The default reference point is written again on the next boot.
* `/status.json` reports `temp.cpu` (`current`, `min`, `max` with raw ADC values) and `cpuTempCal`.

# Supply voltage

`SYS_VOLTAGE` every 34 s: 8 reads of A0 through the board-specific divider (`VCC_DIV_R4/R5`) against the measured 3V3 rail (`MV3_3`). Reported as `vcc` (`current`, `min`, `max`) in `/status.json`. See [controller board](/hardware/controller-circuit.md) for the caveat about the divider.

# Entropy

* `secRandom()`, `secRandom8()` and `secRandom16()` use the RP2350 hardware RNG through `rp2040.hwrand32()`. They are fast; the "about 30 ms" in their doc comments is left over from the ECC608 version.[^util]
* Every 6 min (`RND_ENTROPY`) a `secRandom16()` value goes into `random16_add_entropy`. The saved `randomSeed` in `/state.json` adds entropy at boot.

# Task snapshots

`TASK_SNAPSHOT` every 10 s captures per-task run-time counters. `/tasks.json` computes CPU percentages from the last two snapshots, so it works even without logging.

# Logging

With `LOGGING_ENABLED`, `DIAG_INFO` logs a task summary every 30.25 s through `lib/PicoLog`. PicoLog uses a ring buffer and a dedicated SRL task, so logging rarely blocks the caller. The debug env sets `LOGGING_ENABLED=1`. Release builds turn it on with `build.ps1 -log`.

[^diag]: src/diag.cpp
[^util]: src/util.cpp (secRandom)
