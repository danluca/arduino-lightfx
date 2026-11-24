// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//

#pragma once
#include <Arduino.h>
#include <cstddef>
#include <memory>
#include "fixed_queue.h"
#include "mesh_protocol.h"

// Recipient state is accessed only by the web/comms task.
struct BroadcastClient {
    static constexpr uint8_t BC_ONLINE = 1 << 0;
    static constexpr uint8_t BC_ACTIVE = 1 << 1;
    static constexpr uint8_t BC_STATIC = 1 << 2;

    mesh::Delivery delivery;
    IPAddress ip;
    unsigned long lastSeenMillis = 0;
    uint8_t flags = BC_ACTIVE;  //bitmap of flags - up to 8 boolean flags

    BroadcastClient(const IPAddress &src, const uint8_t lsb) : ip(src) {
        ip[3] = lsb;
    }

    [[nodiscard]] bool isOnline() const { return flags & BC_ONLINE; }
    void setOnline(const bool online) { if (online) flags |= BC_ONLINE; else flags &= ~BC_ONLINE; }
    [[nodiscard]] bool isActive() const { return flags & BC_ACTIVE; }
    void setActive(const bool active) { if (active) flags |= BC_ACTIVE; else flags &= ~BC_ACTIVE; }
    [[nodiscard]] bool isStatic() const { return flags & BC_STATIC; }
    void setStatic(const bool st) { if (st) flags |= BC_STATIC; else flags &= ~BC_STATIC; }
};

using BroadcastRecipients = FixedQueue<std::unique_ptr<BroadcastClient>, 10>;
extern BroadcastRecipients fxBroadcastRecipients;
