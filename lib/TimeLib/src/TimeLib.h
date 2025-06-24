/*
  timelib.h - low level time and date functions
  July 3 2011 - fixed elapsedSecsThisWeek macro (thanks Vincent Valdy for this)
              - fixed  daysToTime_t macro (thanks maniacbug)
  June 2025 - Dan Luca
    - re-designed for robustness, timezone support, NTP sync, thread-safe;
    as a result, there is more code pulled from standard libraries and overall increased flash size
    used by the application. For a RP2040 board with at least 4MB that is not an issue.
    - targeted at Raspberry Pi (Pico) platform - RP2040, RP2350 boards
    - while there is an RTC on-board RP2040, note it doesn't exist in RP2350, which - instead -
    has a powman unit for calendar based sleep/wakeup alarms. The RP2040 RTC is better described
    as Real Time Counter, it lacks proper calendar functionality
*/

#ifndef TIMELIB_H
#define TIMELIB_H

#include "TimeDef.h"
#include "TimeService.h"
#include "TimeFormat.h"

#endif /* TIMELIB_H */
