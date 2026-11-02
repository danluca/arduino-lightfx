// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//
#pragma once
#ifndef TIMEZONE_H_INCLUDED
#define TIMEZONE_H_INCLUDED
#include <CoreTimeCalc.h>
#include "TimeDef.h"

/**
 * @class Timezone
 * @brief A time zone defined by a POSIX TZ rule string (e.g. "CST6CDT,M3.2.0,M11.1.0"), resolved by the C library.
 *
 * The standard C library (newlib) owns the zone rules: the TZ string is installed once with apply() (setenv + tzset) and the
 * conversions use localtime_r()/mktime(), which newlib guards with its retargeted locks (safe across tasks and cores).
 * The framework ships the POSIX strings for all IANA zones in <TZ.h> (TZ_America_Chicago and so on).
 *
 * Limitations (by design, this is an embedded device): one standard offset and at most one DST rule per zone - no historical
 * rule changes, and rule changes need a firmware update. Only one zone can be applied at a time - the C library keeps the zone
 * in a process-wide variable. A zone created without a POSIX rule is plain UTC and never touches the C library state.
 */
class Timezone {
public:
    /**
     * @param name the IANA zone name, for display (e.g. "America/Chicago")
     * @param posixTz the POSIX TZ rule (e.g. TZ_America_Chicago from TZ.h); nullptr or empty means UTC
     */
    explicit Timezone(const char *name, const char *posixTz = nullptr);

    /** Installs this zone as the C library local zone (setenv TZ + tzset). Call before the tasks that use local time are started. */
    void apply() const;

    /** Convert the given UTC time to local time (standard or daylight time, as appropriate) */
    [[nodiscard]] time_t toLocal(const time_t &utc) const;
    /**
     * Convert the given local time to UTC. Local times inside the DST gap resolve per the C library mktime rules, and
     * the repeated hour of the DST-to-standard transition resolves to the first occurrence - avoid relying on those instants.
     */
    [[nodiscard]] time_t toUTC(const time_t &local) const;
    /** Whether the given time falls in daylight saving time */
    [[nodiscard]] bool isDST(const time_t &time, bool bLocal = true) const;
    /** Offset from UTC in seconds for the given time (daylight or standard, as appropriate) */
    [[nodiscard]] int getOffset(const time_t &time, bool bLocal = true) const;
    /** Updates the zone name, offset and dst fields of the time elements structure; the tm fields must hold the broken down *local* time */
    void updateZoneInfo(tmElements_t &tm, const time_t &localTime) const;

    /** The IANA time zone name */
    [[nodiscard]] const char *getName() const { return m_name; }
    /** The POSIX TZ rule string; empty for UTC */
    [[nodiscard]] const char *getPosixTz() const { return m_posixTz; }
    /** Whether this zone is UTC (no rule) */
    [[nodiscard]] bool isUtc() const { return m_posixTz[0] == '\0'; }
    /** The short zone name (abbreviation) in effect at the given time, e.g. CST or CDT */
    [[nodiscard]] const char *getShort(const time_t &time, bool bLocal = true) const;

private:
    struct Resolved {
        int offset;         // seconds east of UTC
        bool dst;
        const char *abbr;
    };
    [[nodiscard]] Resolved resolveUTC(const time_t &utc) const;
    [[nodiscard]] Resolved resolveLocal(const time_t &local) const;

    char m_name[33]{};          // IANA name of the timezone, e.g. "America/New_York", max 32 chars
    char m_posixTz[64]{};       // POSIX TZ rule; the C library keeps its own copy in the environment
};

extern Timezone utcZone;
#endif
