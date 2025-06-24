# Arduino Time Library

Time is a library that provides timekeeping functionality for Arduino Raspberry Pi Pico boards powered by RP2040 and RP2350 chips.

The code is derived from the Playground DateTime library but is updated
to provide an API that is more flexible and easier to use.

A primary goal was to enable a modern, robust and thread-safe functionality. 

## Functionality

To use the Time library in an Arduino sketch, include TimeLib.h.

```c++
#include <TimeLib.h>
```
Then in the source code define a UDP client (needed for NTP updates) and a global `TimeService` instance: 
```c++
WiFiUDP udp;
TimeService timeService(&udp);
```
If a timezone other than UTC (default) is needed:

```c++
constexpr TimeChangeRule cdt {.name = "CDT", .week = Second, .dow = Sun, .month = Mar, .hour = 2, .offset = -300};
constexpr TimeChangeRule cst {.name = "CST", .week = First, .dow = Sun, .month = Nov, .hour = 2, .offset = -360};
const Timezone centralTime(cdt, cst, "America/Chicago");
...
timeService.applyTimezone(centralTime);
```

The functions available in the library include

```c++
hour();            // the hour now  (0-23)
minute();          // the minute now (0-59)
second();          // the second now (0-59)
day();             // the day now (1-31)
weekday();         // day of the week (1-7), Sunday is day 1
month();           // the month now (1-12)
year();            // the full four digit year: (2009, 2010 etc)
now();             // the local time_t value - seconds since 1/1/1970, in local time
utcNow();          // the UTC time_t value - seconds since 1/1/1970, in UTC time
nowMilis();        // similar with now(), but value is in milliseconds
utcNowMilis();     // similar with utcNow(), but value is in milliseconds
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

Time and Date values are not valid if the status is `timeNotSet`. Otherwise, values can be used but
the returned time may have drifted if the status is `timeNeedsSync`.

There are many convenience macros in the `TimeDef.h` file for time constants and conversion
of time units.

## Technical Notes

Internal system time is based on the standard Unix `time_t`, kept in milliseconds since the Unix Epoch (Jan 1, 1970).
The standard unit for Unix time is the number of seconds since Jan 1, 1970. System time begins at zero when the sketch starts.

Low-level functions to convert between system time and individual time elements are provided:

```c++
breakTime(time, &tm);  // break time_t into elements stored in tm struct
makeTime(&tm);         // return time_t from elements stored in tm struct
```

# Arduino Timezone Library
https://github.com/JChristensen/Timezone  
README file  
Jack Christensen  
Mar 2012

## License
Arduino Timezone Library Copyright (C) 2018 Jack Christensen GNU GPL v3.0

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License v3.0 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/gpl.html>

## Introduction
The **Timezone** library is designed to work in conjunction with the Time service. 

The primary aim of the **Timezone** library is to convert Universal Coordinated Time (UTC) to the correct local time, whether it is daylight saving time (a.k.a. summer time) or standard time. The time source could be a GPS receiver, an NTP server, or a Real-Time Clock (RTC) set to UTC.  But whether a hardware RTC or other time source is even present is immaterial, since the Time library can function as a software RTC without additional hardware (although its accuracy is dependent on the accuracy of the microcontroller's system clock.)

The **Timezone** library implements two objects to facilitate time zone conversions:
- A **TimeChangeRule** object describes when local time changes to daylight (summer) time, or to standard time, for a particular locale.
- A **Timezone** object uses **TimeChangeRule**s to perform conversions and related functions.  It can also write its **TimeChangeRule**s to EEPROM, or read them from EEPROM.  Multiple time zones can be represented by defining multiple **Timezone** objects.

## Coding TimeChangeRules
Normally these will be coded in pairs for a given time zone: One rule to describe when daylight (summer) time starts, and one to describe when standard time starts.

As an example, here in the Eastern US time zone, Eastern Daylight Time (EDT) starts on the 2nd Sunday in March at 02:00 local time. Eastern Standard Time (EST) starts on the 1st Sunday in November at 02:00 local time.

Define a **TimeChangeRule** as follows:

`TimeChangeRule myRule = {abbrev, week, dow, month, hour, offset};`

Where:

**abbrev** is a character string abbreviation for the time zone; it must be no longer than five characters.

**week** is the week of the month that the rule starts.

**dow** is the day of the week that the rule starts.

**hour** is the hour in local time that the rule starts (0-23).

**offset** is the UTC offset _in minutes_ for the time zone being defined.

For convenience, the following symbolic names can be used:

**week:** First, Second, Third, Fourth, Last  
**dow:** Sun, Mon, Tue, Wed, Thu, Fri, Sat  
**month:** Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec

For the Eastern US time zone, the **TimeChangeRule**s could be defined as follows:

```c++
TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  //UTC - 4 hours
TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   //UTC - 5 hours
```

## Coding Timezone objects
There are three ways to define **Timezone** objects.

By first defining **TimeChangeRule**s (as above) and giving the daylight time rule and the standard time rule (assuming usEDT and usEST defined as above):  
`Timezone usEastern(usEDT, usEST, "America/New_York");`

For a time zone that does not change to daylight/summer time, pass a single rule to the constructor. For example:  
`Timezone usAZ(usMST, usMST, "America/Phoenix);`

Note that **TimeChangeRule**s require 12 bytes of storage each, so the pair of rules associated with a Timezone object requires 24 bytes total.  This could possibly change in future versions of the library.  The size of a **TimeChangeRule** can be checked with `sizeof(usEDT)`.
