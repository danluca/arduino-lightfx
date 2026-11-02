# Arduino Time Library

Time is a library that provides timekeeping functionality for Arduino Raspberry Pi Pico boards powered by RP2040 and RP2350 chips.

The code is derived from the Playground DateTime library but is updated
to provide an API that is more flexible and easier to use.

The library does not keep a clock of its own. It is a thin layer over the platform primitives of the arduino-pico framework:

* the UTC clock is the **system clock** (`gettimeofday()`); there is no battery-backed RTC, so it starts from 1970 at boot until it is set
* NTP is the framework's **lwIP SNTP client**, which polls in the background and sets the system clock
* local time and DST come from the C library (**newlib**), driven by a **POSIX TZ string**

## Functionality

To use the Time library in an Arduino sketch, include TimeLib.h.

```c++
#include <TimeLib.h>
```
Then in the source code define a global `TimeService` instance (the library refers to it as `timeService`):
```c++
TimeService timeService;
```
If a timezone other than UTC (default) is needed, define a `Timezone` from an IANA name and a POSIX TZ rule. The framework
provides the rules of all IANA zones in `TZ.h`:

```c++
#include <TZ.h>
Timezone centralTime("America/Chicago", TZ_America_Chicago);   // "CST6CDT,M3.2.0,M11.1.0"
...
timeService.applyTimezone(centralTime);     // call once, early; installs the zone in the C library (setenv TZ + tzset)
```

To keep the system clock in sync with an NTP server, once the network is up:

```c++
timeService.beginNTP(IPAddress(192, 168, 0, 58));     // or beginNTP("pool.ntp.org") - resolves the name once
if (timeService.waitSynchronized(10000)) { ... }      // optionally wait for the first sync
timeService.isSynchronized();                         // the clock was set by NTP at least once
timeService.lastSyncAge();                            // seconds since the last sync, -1 if none
```

### Build requirements

The lwIP SNTP code is compiled with the framework, so its settings are `build_flags` in `platformio.ini`, not `lwipopts.h` entries:

```ini
-D SNTP_UPDATE_DELAY=43200000UL      ; poll interval in ms (lwIP default is 1 hour)
-Wl,--wrap=__setSystemTime           ; routes the SNTP clock update through TimeLib's validating hook
```

The hook (`__wrap___setSystemTime`) ignores times outside 2020-01-01..2070-01-01, and counts the accepted and rejected syncs.
The `tm_offset` and `tm_zone` fields of `struct tm` require `-D __TM_GMTOFF=tm_offset -D __TM_ZONE=tm_zone`.

### Functions

The functions available in the library include

```c++
hour();            // the hour now  (0-23)
minute();          // the minute now (0-59)
second();          // the second now (0-59)
day();             // the day now (1-31)
weekday();         // day of the week (0-6), Sunday is day 0
month();           // the month now (0-11)
year();            // the full four digit year: (2009, 2010 etc)
now();             // the local time_t value - seconds since 1/1/1970, in local time
utcNow();          // the UTC time_t value - seconds since 1/1/1970, in UTC time
nowMillis();       // similar with now(), but value is in milliseconds
utcNowMillis();    // similar with utcNow(), but value is in milliseconds
```

there are also functions to return the hour in 12-hour format

```c++
hourFormat12();    // the hour now in 12 hour format
isAM();            // returns true if time now is AM
isPM();            // returns true if time now is PM
```

The time and date functions can take an optional parameter for the time. This prevents
errors if the time rolls over between elements. For example, if a new minute begins
between getting the minute and second, the values will be inconsistent. Using the
following functions eliminates this problem

```c++
time_t t = now(); // store the current time in time variable t
hour(t);          // returns the hour for the given time t
minute(t);        // returns the minute for the given time t
second(t);        // returns the second for the given time t
day(t);           // the day for the given time t
weekday(t);       // day of the week for the given time t
month(t);         // the month for the given time t
year(t);          // the year for the given time t
```

Until the first NTP sync the clock is not valid (it counts from 1970 at boot): check `timeService.isSynchronized()`.
Between syncs the clock runs on the chip crystal and drifts by seconds per day; each SNTP sync steps it back to the server's time.

There are many convenience macros in the `TimeDef.h` file for time constants and conversion
of time units.

## Time zones

`Timezone` wraps a POSIX TZ rule string and asks the C library (`localtime_r`, `mktime`, `tzname`) for the offset, the DST flag and
the zone abbreviation:

```c++
centralTime.isDST(t);          // t is local time by default; pass false as 2nd argument for UTC
centralTime.getOffset(t);      // seconds from UTC, DST included
centralTime.getShort(t);       // "CST" or "CDT"
centralTime.toLocal(utc);
centralTime.toUTC(local);      // times inside the DST gap, or the repeated hour, are ambiguous
```

Limits, by design for an embedded device: one standard offset and at most one DST rule per zone, no historical rule changes, and
rule changes need a firmware update (new `TZ.h`). Only one zone can be active, because the C library keeps it in a process-wide
variable. A `Timezone` created without a rule (`utcZone`) is UTC.

## Technical Notes

Internal system time is based on the standard Unix `time_t` (64 bits). The `now()` family returns local time, expressed as
"seconds since the Unix Epoch (Jan 1, 1970) as if local time were UTC"; `utcNow()` returns UTC.

Low-level functions to convert between system time and individual time elements are provided:

```c++
timeService.breakTime(time, tm);   // break local time_t into elements stored in tm struct, with zone offset and name
TimeService::makeTime(tm);         // return time_t from elements stored in tm struct
```

## Credits

The time functions derive from the Arduino Time library by Michael Margolis and Paul Stoffregen (LGPL v2.1).
The earlier `Timezone` class and `TimeChangeRule` structure of this library were based on the Arduino Timezone Library by
Jack Christensen (https://github.com/JChristensen/Timezone, GNU GPL v3.0); they have been replaced by the C library time zone support.
