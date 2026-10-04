---
type: Issue
title: Wi-Fi time fallback
description: When NTP fails, handleNTPFailure() sets the clock from WiFi.getTime(). On the CYW43 build of arduino-pico that call returns millis(), so the clock jumps to a date in the early 1970s, which skews holiday selection and time-of-day dimming until NTP succeeds.
tags: [bug, time, ntp, wifi, cyw43]
severity: medium
issue_state: fixed
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: time
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/timeutil.cpp
    title: src/timeutil.cpp (timeSetup, handleNTPFailure)
  - id: wifi
    resource: https://github.com/earlephilhower/arduino-pico/blob/master/libraries/WiFi/src/WiFiClass.cpp
    title: arduino-pico libraries/WiFi/src/WiFiClass.cpp (getTime)
  - id: comms
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/comms.cpp
    title: src/comms.cpp (timeSetupCheck)
---

# Defect

`timeSetup()` no longer tries Wi-Fi time itself; its comment says the Wi-Fi module "does not grab NTP time" on this board. But when NTP fails it still calls `handleNTPFailure()`, which does:[^time]

```cpp
if (const time_t wifiTime = WiFi.getTime(); wifiTime > 0) {
    timeService.setTime(wifiTime);
    ...
```

In the framework's `WiFiClass`, `getTime()` is `return millis();`.[^wifi] That value is always above 0 after boot, so the branch always runs and sets the clock to "uptime in milliseconds, read as seconds since 1970". Five minutes of uptime becomes 1970-01-04; a day of uptime becomes a date in 1972.

# When it happens

* At boot, when the NTP server (`192.168.0.58` with `LOCAL_NTP_SERVER`) does not answer.
* On every 60-second retry (`timeSetupCheck` → `timeSetup`) until NTP succeeds, so the clock jumps again on each retry.[^comms]
* Not after a later re-sync failure: `timeUpdate` clears `Ntp` but does not call `handleNTPFailure`, so a board that once had good time keeps its clock.

# Impact

* Holiday auto-detection runs on the bogus date (`adjustHoliday` only needs `Wifi`), so the board picks the theme and effect weights for an early-January date: `NewYear` or `Party`.
* Time-of-day [dimming](/scheduling/brightness-dimming.md) uses `hour()` of the bogus time, so the strip may dim in daytime or not at night.
* Log timestamps are re-based to the bogus time.
* Sleep alarms are not affected, because `ALARM_SETUP` waits for a real NTP sync.

# Fix

Remove the `WiFi.getTime()` branch from `handleNTPFailure()` on this board (keep the error log and the unsynchronized clock), or guard it with a sanity check such as `wifiTime > TWENTY_TWENTY`. `TWENTY_TWENTY` is already defined in `util.h`.

# Resolution

Fixed in the working tree (after `6d11a87`, not yet committed): `handleNTPFailure()` no longer calls `WiFi.getTime()`. It clears `Ntp`, logs an error and leaves the clock and holiday untouched; the 60-second `timeSetup` timer retries NTP.

# Verification status

Found by reading the source and the framework's `WiFiClass::getTime()`. Not reproduced on hardware. The fix builds for both boards (`rp2350-rel`, with and without logging); not yet tested on a board.

[^time]: src/timeutil.cpp (timeSetup, handleNTPFailure)
[^wifi]: arduino-pico libraries/WiFi/src/WiFiClass.cpp (getTime)
[^comms]: src/comms.cpp (timeSetupCheck)
