// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//

// Shared RP2040/RP2350 wire protocol and allocation-free delivery state.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace mesh {
inline constexpr uint32_t protocolVersion = 1;
inline constexpr uint32_t announcementMs = 60000;
inline constexpr uint32_t leaseMs = 180000;
inline constexpr uint32_t retryMs = 1000;
inline constexpr uint8_t maxAttempts = 3;
inline constexpr size_t maxBodyBytes = 512;
inline constexpr size_t maxStateBytes = maxBodyBytes - 32; // Leave room for acknowledgment status.

/**
 * Mesh protocol state.
 */
struct State {
    char master[65]{};
    char source[65]{};
    char session[33]{};
    char effect[33]{};
    uint32_t sequence = 0;
};

enum class Order { New, Duplicate, Stale, Conflict };

/**
 * Mesh protocol inbox.
 */
class Inbox {
    State state_{};
    uint32_t ticket_ = 0;
    bool active_ = false;
    char retiredSession_[33]{};
public:
    bool active() const { return active_; }
    const State &state() const { return state_; }
    uint32_t ticket() const { return ticket_; }
    void clear() { active_ = false; }
    Order inspect(const State &incoming) const {
        if (!active_) return Order::New;
        if (std::strcmp(state_.master, incoming.master)) return Order::Conflict;
        if (std::strcmp(state_.session, incoming.session))
            return std::strcmp(retiredSession_, incoming.session) ? Order::New : Order::Stale;
        const uint32_t delta = incoming.sequence - state_.sequence;
        if (delta == 0)
            return std::strcmp(state_.effect, incoming.effect) ? Order::Conflict : Order::Duplicate;
        return delta < 0x80000000u ? Order::New : Order::Stale;
    }
    // Commit only after the command has entered the FX queue successfully.
    void commit(const State &state, uint32_t ticket) {
        if (active_ && std::strcmp(state_.session, state.session))
            std::memcpy(retiredSession_, state_.session, sizeof(retiredSession_));
        state_ = state;
        ticket_ = ticket;
        active_ = true;
    }
};

/**
 * Mesh protocol delivery state.
 */
class Delivery {
    uint32_t sequence_ = 0;
    uint32_t lastAttemptMs_ = 0;
    uint32_t lastAppliedMs_ = 0;
    uint8_t attempts_ = 0;
    bool pending_ = false;
    bool confirmed_ = false;
public:
    uint32_t sequence() const { return sequence_; }
    void start(uint32_t sequence) {
        sequence_ = sequence;
        attempts_ = 0;
        pending_ = true;
    }
    bool due(uint32_t now) const {
        return pending_ && attempts_ < maxAttempts &&
            (attempts_ == 0 || now - lastAttemptMs_ >= retryMs);
    }
    void attempted(uint32_t now) { lastAttemptMs_ = now; ++attempts_; }
    void applied(uint32_t now) { lastAppliedMs_ = now; confirmed_ = true; pending_ = false; }
    void rejected() { confirmed_ = false; pending_ = false; }
    bool confirmed(uint32_t now) const { return confirmed_ && now - lastAppliedMs_ < leaseMs; }
};
}
