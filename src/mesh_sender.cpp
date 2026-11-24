// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//

#include "mesh_sender.h"
#include "broadcast_client.h"
#include "mesh_http.h"
#include "efx_setup.h"
#include "comms.h"
#include "sysinfo.h"
#include "log.h"
#include <pico/rand.h>
#include <cstdio>
#include <cstring>

/**
 * @brief Services outbound mesh state delivery to configured broadcast recipients.
 *
 * Announces the currently published effect to mesh clients when broadcasting is
 * enabled and Wi-Fi is available. The function initializes a mesh session when
 * broadcasting starts, builds or refreshes the current state announcement when
 * the published effect changes or the announcement interval elapses, and
 * schedules delivery attempts for each recipient.
 *
 * Delivery is performed incrementally. At most one eligible peer is contacted per
 * invocation so that the caller can continue servicing HTTP, communications, and
 * watchdog work between retries. Peers that acknowledge the announced state are
 * marked active and updated with their last-seen timestamp. Peers that reject the
 * state with a terminal HTTP status are marked inactive and their delivery is
 * rejected. Transient responses, pending acknowledgments, malformed or mismatched
 * acknowledgments, queue-full responses, and network failures remain eligible for
 * retry according to each peer delivery schedule.
 *
 * Broadcasting is disabled immediately when the global broadcast flag is cleared.
 * No announcement is sent unless a publishable effect is available, Wi-Fi is up,
 * the device identity and name fit in the mesh state fields, and the effect ID is
 * valid for the announcement payload.
 *
 * @note This function is intended to be called repeatedly from the communications
 * loop.
 * @note The function retains session, announcement, timing, and round-robin cursor
 * state between calls.
 */
void serviceMeshClients() {
    static bool enabled = false;
    static mesh::State announcement;
    static uint32_t lastAnnouncement = 0;
    static uint16_t lastEffect = 0;
    static size_t cursor = 0;
    uint16_t index = 0;
    if (!fxBroadcastEnabled.load()) {
        enabled = false;
        log_debug(F("Mesh broadcast disabled - no servicing clients"));
        return;
    }
    if (!meshPublishedEffect(index) || !sysInfo->isSysStatus(SysStatus::Wifi)) {
        log_info(F("Mesh broadcast disabled - no effect [index = %d] or Wi-Fi (%s)"), index, sysInfo->isSysStatus(SysStatus::Wifi) ? "up" : "down");
        return;
    }
    const uint32_t nowMs = millis();
    const bool starting = !enabled;
    if (starting) {
        announcement = {};
        const uint64_t nonce = get_rand_64();
        snprintf(announcement.session, sizeof(announcement.session), "%08lx%08lx", static_cast<unsigned long>(nonce >> 32), static_cast<unsigned long>(nonce));
        const String &id = sysInfo->getBoardId();
        const String &name = sysInfo->getDeviceName();
        if (!id.length() || !name.length() || id.length() >= sizeof(announcement.master) ||
            name.length() >= sizeof(announcement.source)) return;
        std::memcpy(announcement.master, id.c_str(), id.length() + 1);
        std::memcpy(announcement.source, name.c_str(), name.length() + 1);
        enabled = true;
    }
    if (starting || index != lastEffect || nowMs - lastAnnouncement >= mesh::announcementMs) {
        const String effect = fxRegistry.getEffectId(index);
        if (effect == "unknown" || effect.length() >= sizeof(announcement.effect)) return;
        std::memcpy(announcement.effect, effect.c_str(), effect.length() + 1);
        ++announcement.sequence;
        lastEffect = index;
        lastAnnouncement = nowMs;
        for (auto &peer : fxBroadcastRecipients) {
            if (starting) peer->delivery = {};
            peer->delivery.start(announcement.sequence);
        }
    }
    const size_t count = fxBroadcastRecipients.size();
    // One attempt per web/comms iteration: service HTTP and watchdog between peers/retries.
    for (size_t n = 0; n < count; ++n) {
        cursor %= count;
        auto &peer = *(fxBroadcastRecipients.begin() + cursor++);
        if (peer->delivery.sequence() != announcement.sequence)
            peer->delivery.start(announcement.sequence);
        if (!peer->delivery.due(nowMs)) continue;
        String body, response;
        if (!meshEncodeState(announcement, body)) return;
        peer->delivery.attempted(nowMs);
        const int status = meshExchange(peer->ip, body, response);
        peer->setOnline(status > 0);
        if (status == 200 && meshAppliedAck(response, announcement)) {
            peer->delivery.applied(millis());
            peer->setActive(true);
            peer->lastSeenMillis = millis();
            log_info(F("Mesh state applied by %s, cursor %d"), peer->ip.toString().c_str(), cursor);
        } else if (status == 400 || status == 403 || status == 409 || status == 422 || status == 404) {
            peer->delivery.rejected();
            peer->setActive(false);
            log_warn(F("Mesh state rejected by %s, cursor %d: HTTP %d"), peer->ip.toString().c_str(), cursor, status);
        }
        // 202, queue-full, malformed/mismatched acknowledgments and network failures retry.
        return;
    }
}
