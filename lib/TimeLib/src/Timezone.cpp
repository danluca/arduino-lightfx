/*----------------------------------------------------------------------*
 * Arduino Timezone Library                                             *
 * Jack Christensen Mar 2012                                            *
 *                                                                      *
 * Arduino Timezone Library Copyright (C) 2018,2025 by Jack Christensen and  *
 * licensed under GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html   *
 *----------------------------------------------------------------------*/
//
// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "Timezone.h"
#include "TimeService.h"

struct dstTransitions {
    time_t m_dstUTC{};        // dst start for the given / current year, given in UTC
    time_t m_stdUTC{};        // std time start for given/current year, given in UTC
    time_t m_dstLoc{};        // dst start for the given/current year, given in local time
    time_t m_stdLoc{};        // std time start for given/current year, given in local time
};

static dstTransitions currentTransitions;  // current DST transitions for the last year inquired
static mutex_t currentTransitionsMutex;    // mutex for currentTransitions
static constexpr TimeChangeRule UTC_RULE = { "UTC", Last, Sun, Jan, 0, 0 };
const Timezone utcZone(UTC_RULE, UTC_RULE, UTC_RULE.name);                               // the UTC timezone, used for UTC time conversions

/**
 * Convert the given time change rule to a transition time_t value for the given year.
 * @param r the rule
 * @param yr year of interest
 * @return the transition time for the year given
 */
static time_t transitionTime(const TimeChangeRule &r, int yr) {
    uint8_t m = r.month;     // temp copies of r.month and r.week
    uint8_t w = r.week;
    if (w == 0) {            // is this a "Last week" rule?
        if (++m > 12) {      // yes, for "Last", go to the next month
            m = 1;
            ++yr;
        }
        w = 1;               // and treat as the first week of next month, subtract 7 days later
    }

    // calculate the first day of the month, or for "Last" rules, first day of the next month
    tmElements_t tm;
    tm.tm_hour = r.hour;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_mday = 1;
    tm.tm_mon = m;
    tm.tm_year = yr;
    time_t t = mktime(&tm);

    // add offset from the first of the month to r.dow, and offset for the given week
    t += ( (r.dow - weekday(t) + 7) % 7 + (w - 1) * 7 ) * SECS_PER_DAY;
    // back up a week if this is a "Last" rule
    if (r.week == 0) t -= 7 * SECS_PER_DAY;
    return t;
}

/**
 * Calculate the DST and standard time change points for the given year as local and UTC time_t values,
 * provided the DST and STD rules are different (DST is observed)
 * @param dst the DST rule
 * @param std the Standard rule
 * @param year year of interest
 * @return a structure with time transition points; if DST is not observed, the fields are zeroed out
 */
static dstTransitions calcTimeChanges(const TimeChangeRule &dst, const TimeChangeRule &std, const int year) {
    dstTransitions dstLoc;
    // if (dst.offset == std.offset)
        // return dstLoc;    // DST is not observed in this timezone

    dstLoc.m_dstLoc = transitionTime(dst, year);
    dstLoc.m_stdLoc = transitionTime(std, year);
    dstLoc.m_dstUTC = dstLoc.m_dstLoc - std.offset * SECS_PER_MIN;
    dstLoc.m_stdUTC = dstLoc.m_stdLoc - dst.offset * SECS_PER_MIN;
    return dstLoc;
}

/**
 * Create a Timezone object from the given time change rules.
 * @param dstStart
 * @param stdStart
 * @param name
 */
Timezone::Timezone(const TimeChangeRule &dstStart, const TimeChangeRule &stdStart, const char* name): m_dst(dstStart), m_std(stdStart) {
    strncpy(m_name, name, sizeof(m_name)-1);
}

/**
 * Create a Timezone object for a zone that does not observe daylight time.
 * @param stdTime
 * @param name
 */
Timezone::Timezone(const TimeChangeRule &stdTime, const char* name) : m_dst(stdTime), m_std(stdTime) {
    strncpy(m_name, name, sizeof(m_name)-1);
}

/**
 * Convert the given UTC time to local time, standard or daylight time, as appropriate.
 * @param utc
 * @return
 */
time_t Timezone::toLocal(const time_t utc) const {
    if (isDST(utc, false))
        return utc + m_dst.offset * SECS_PER_MIN;
    return utc + m_std.offset * SECS_PER_MIN;
}


/**
 * Convert the given local time to UTC time.
 *
 * WARNING:
 * This function is provided for completeness, but should seldom be
 * needed and should be used sparingly and carefully.
 *
 * Ambiguous situations occur after the Standard-to-DST and the
 * DST-to-Standard time transitions. When changing to DST, there is
 * one hour of local time that does not exist, since the clock moves
 * forward one hour. Similarly, when changing to standard time, there
 * is one hour of local times that occur twice since the clock moves
 * back one hour.
 *
 * This function does not test whether it is passed an erroneous time
 * value during the Local -> DST transition that does not exist.
 * If passed such a time, an incorrect UTC time value will be returned.
 *
 * If passed a local time value during the DST -> Local transition that occurs
 * twice, it will be treated as the earlier time, i.e. the time that occurs before
 * the transition.
 *
 * Calling this function with local times during a transition interval should be avoided!
 * @param local
 * @return
 */
time_t Timezone::toUTC(const time_t local) const {
    if (isDST(local, true))
        return local - m_dst.offset * SECS_PER_MIN;
    return local - m_std.offset * SECS_PER_MIN;
}

/**
 * Determine whether the given time_t is within the DST interval or the Standard time interval
 * @param time time to check - seconds
 * @param bLocal whether the time to check is in local time (default) or UTC
 * @return if the given time falls during the DST period and DST is observed; false otherwise
 */
bool Timezone::isDST(const time_t time, const bool bLocal) const {
    if (!isDSTObserved())
        return false;

    // recalculate the time change points if needed
    if (const int yr = year(time); yr != year(bLocal ? currentTransitions.m_dstLoc : currentTransitions.m_dstUTC)) {
        CoreMutex mutex(&currentTransitionsMutex);
        currentTransitions = calcTimeChanges(m_dst, m_std, yr);
    }

    //time is local
    if (bLocal) {
        // Northern Hemisphere
        if (currentTransitions.m_stdLoc > currentTransitions.m_dstLoc)
            return (time >= currentTransitions.m_dstLoc && time < currentTransitions.m_stdLoc);
        // Southern Hemisphere
        return !(time >= currentTransitions.m_stdLoc && time < currentTransitions.m_dstLoc);
    }

    //time is in utc
    // Northern Hemisphere
    if (currentTransitions.m_stdUTC > currentTransitions.m_dstUTC)
        return time >= currentTransitions.m_dstUTC && time < currentTransitions.m_stdUTC;
    // Southern Hemisphere
    return !(time >= currentTransitions.m_stdUTC && time < currentTransitions.m_dstUTC);
}


