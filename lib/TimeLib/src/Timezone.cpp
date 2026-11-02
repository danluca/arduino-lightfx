// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//

#include <cstdlib>
#include <cstring>
#include <ctime>
#include "Timezone.h"

Timezone utcZone("UTC");    // the UTC timezone, used for UTC time conversions and as default until a zone is applied

Timezone::Timezone(const char *name, const char *posixTz) {
    strncpy(m_name, name, sizeof(m_name) - 1);
    if (posixTz != nullptr)
        strncpy(m_posixTz, posixTz, sizeof(m_posixTz) - 1);
}

void Timezone::apply() const {
    if (isUtc())
        return;
    setenv("TZ", m_posixTz, 1);
    tzset();
}

/**
 * Resolves the zone details in effect at the given UTC instant, using the C library rules.
 * The offset is derived from the broken down local time, as newlib does not populate the tm GMT offset field.
 */
Timezone::Resolved Timezone::resolveUTC(const time_t &utc) const {
    if (isUtc())
        return {0, false, "UTC"};
    time_t t = utc;
    tmElements_t lt{};
    localtime_r(&t, &lt);
    const bool dst = lt.tm_isdst > 0;
    const int offset = static_cast<int>(CoreTimeCalc::makeTimeCore(lt) - utc);  // the local fields read as if they were UTC
    return {offset, dst, tzname[dst ? 1 : 0]};
}

/**
 * Resolves the zone details for the given local time (seconds since the epoch, with the local fields read as if they were UTC)
 */
Timezone::Resolved Timezone::resolveLocal(const time_t &local) const {
    if (isUtc())
        return {0, false, "UTC"};
    tmElements_t lt{};
    CoreTimeCalc::breakTimeCore(local, lt);
    lt.tm_isdst = -1;   // let the C library decide whether DST applies to this local time
    const time_t utc = mktime(&lt);
    if (utc == static_cast<time_t>(-1) && local != -1)   // out of range - fall back on the offset resolved at the face value
        return resolveUTC(local);
    const bool dst = lt.tm_isdst > 0;
    return {static_cast<int>(local - utc), dst, tzname[dst ? 1 : 0]};
}

time_t Timezone::toLocal(const time_t &utc) const {
    return utc + resolveUTC(utc).offset;
}

time_t Timezone::toUTC(const time_t &local) const {
    return local - resolveLocal(local).offset;
}

bool Timezone::isDST(const time_t &time, const bool bLocal) const {
    return (bLocal ? resolveLocal(time) : resolveUTC(time)).dst;
}

int Timezone::getOffset(const time_t &time, const bool bLocal) const {
    return (bLocal ? resolveLocal(time) : resolveUTC(time)).offset;
}

const char *Timezone::getShort(const time_t &time, const bool bLocal) const {
    return (bLocal ? resolveLocal(time) : resolveUTC(time)).abbr;
}

void Timezone::updateZoneInfo(tmElements_t &tm, const time_t &localTime) const {
    const auto [offset, dst, abbr] = resolveLocal(localTime);
    tm.tm_isdst = dst;
    tm.tm_offset = offset;
    tm.tm_zone = abbr;
}
