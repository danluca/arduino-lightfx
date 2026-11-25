// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//

#include "mesh_http.h"
#include "mesh_fx_session.h"
#include "efx_setup.h"
#include "task_msg.h"
#include "web_server.h"
#include "comms.h"
#include "config.h"
#include "log.h"

std::atomic<uint32_t> meshAppliedTicket{0};
static std::atomic<uint16_t> publishedEffect{0};
static std::atomic<bool> publishedReady{false};
String masterBoardName;
static MeshFxActivity controlActivity;
static IPAddress controlIp;
static mesh::Inbox inbox;
static uint32_t nextTicket = 0;

/**
 * Publish an effect to the mesh node.
 *
 * @param index Index of the effect to publish.
 */
void meshPublishEffect(const uint16_t index) {
    publishedEffect.store(index, std::memory_order_relaxed);
    publishedReady.store(true, std::memory_order_release);
}

/**
 * Retrieve the index of the effect published.
 *
 * @param index Index of the effect published.
 * @return True if an effect was published, false otherwise.
 */
bool meshPublishedEffect(uint16_t &index) {
    if (!publishedReady.load(std::memory_order_acquire)) return false;
    index = publishedEffect.load(std::memory_order_relaxed);
    return true;
}

/**
 * Clear the master control state.
 */
void clearMasterControl() {
    controlActivity.clear();
    inbox.clear();
    controlIp = IPAddress();
    masterBoardName = "";
}

bool meshControlActive() { return controlActivity.active(); }
bool meshVersionedControlActive() { return inbox.active(); }

/**
 * Prepare the mesh node to become the master.
 *
 * @return True if the mesh node is ready to become the master, false otherwise.
 */
bool meshPrepareMaster() {
    if (!controlActivity.active()) return true;
    const FxActionMessage msg{MESH_FX_RESTORE, 0};
    if (xQueueSend(fxQueue, &msg, 0) != pdPASS) return false;
    clearMasterControl();
    return true;
}

/**
 * Record the master control state.
 *
 * @param ip IP address of the master control.
 * @param name Name of the master control.
 */
void recordMasterControl(const IPAddress &ip, const String &name) {
    controlActivity.record(millis());
    controlIp = ip;
    masterBoardName = name;
}

/**
 * Check if the master control has timed out. If it has, restore the local FX state (sends a restore message).
 *
 */
void checkMasterControlTimeout() {
    const uint32_t timeout = inbox.active() ? mesh::leaseMs : MESH_CONTROL_TIMEOUT_MINUTES * 60000UL;
    if (!controlActivity.timedOut(millis(), timeout)) return;
    constexpr FxActionMessage msg{MESH_FX_RESTORE, 0};
    if (xQueueSend(fxQueue, &msg, 0) == pdPASS) {
        log_warn(F("Mesh master %s timed out; restoring local FX state"), masterBoardName.c_str());
        clearMasterControl();
    }
}

/**
 * Describe the mesh control state.
 *
 * @param master JSON object to describe the mesh control state.
 */
void describeMeshControl(JsonObject master) {
    if (!controlActivity.active()) return;
    const bool applied = !inbox.active() || meshAppliedTicket.load(std::memory_order_acquire) == inbox.ticket();
    master["controlState"] = applied ? "applied" : "queued";
    master["protocol"] = inbox.active() ? mesh::protocolVersion : 0;
    if (applied) master["masterBoard"] = masterBoardName;
}

/**
 * Read a text value from a JSON variant.
 *
 * @param value JSON variant to read the text value from.
 * @param dest Destination buffer to store the text value.
 * @return True if the text value was successfully read, false otherwise.
 */
template<size_t N> static bool readText(JsonVariantConst value, char (&dest)[N]) {
    if (!value.is<const char *>()) return false;
    const char *text = value.as<const char *>();
    const size_t length = std::strlen(text);
    if (!length || length >= N) return false;
    std::memcpy(dest, text, length + 1);
    return true;
}

/**
 * Describe the mesh control state.
 *
 * @param doc JSON document to describe the mesh control state.
 * @param state Mesh control state to describe.
 * @param type Type of the mesh control state.
 */
static void envelope(JsonDocument &doc, const mesh::State &state, const char *type) {
    doc["protocol"] = mesh::protocolVersion;
    doc["type"] = type;
    doc["master"] = state.master;
    doc["source"] = state.source;
    doc["session"] = state.session;
    doc["sequence"] = state.sequence;
    doc["effect"] = state.effect;
}

/**
 * Encode the mesh control state as a JSON string.
 *
 * @param state Mesh control state to encode.
 * @param body Destination string to store the encoded JSON string.
 * @return True if the mesh control state was successfully encoded, false otherwise.
 */
bool meshEncodeState(const mesh::State &state, String &body) {
    JsonDocument doc;
    envelope(doc, state, "state");
    if (doc.overflowed() || measureJson(doc) > mesh::maxStateBytes) return false;
    body = "";
    return serializeJson(doc, body) != 0;
}

/**
 * Decode a mesh control state from a JSON string.
 *
 * @param body JSON string to decode the mesh control state from.
 * @param state Destination mesh control state to store the decoded state.
 * @return True if the mesh control state was successfully decoded, false otherwise.
 */
bool meshAppliedAck(const String &body, const mesh::State &state) {
    if (body.length() > mesh::maxBodyBytes) return false;
    JsonDocument doc;
    if (deserializeJson(doc, body)) return false;
    return doc["protocol"].is<uint32_t>() && doc["protocol"].as<uint32_t>() == mesh::protocolVersion &&
        doc["type"] == "ack" && doc["status"] == "applied" &&
        doc["master"] == state.master && doc["session"] == state.session &&
        doc["sequence"].is<uint32_t>() && doc["sequence"].as<uint32_t>() == state.sequence &&
        doc["effect"] == state.effect;
}

/**
 * Send a mesh control state response.
 *
 * @param client WebClient to send the response to.
 * @param state Mesh control state to send.
 * @param code HTTP status code to send.
 * @param status Status message to send.
 */
static void reply(WebClient &client, const mesh::State &state, int code, const char *status) {
    JsonDocument doc;
    envelope(doc, state, "ack");
    doc.remove("source"); // Keep acknowledgments smaller than their request plus status.
    doc["status"] = status;
    String body;
    serializeJson(doc, body);
    client.addResponseHeader("Cache-Control", "no-store");
    client.send(code, "application/json", body);
}

/**
 * Handle a mesh control state request.
 *
 * @param client WebClient to handle the request for.
 */
void handleMeshState(WebClient &client) {
    const String body = client.request().body();
    mesh::State state;
    JsonDocument doc;
    if (body.length() > mesh::maxStateBytes || deserializeJson(doc, body) ||
        !doc["protocol"].is<uint32_t>() || doc["protocol"].as<uint32_t>() != mesh::protocolVersion ||
        doc["type"] != "state" || !doc["sequence"].is<uint32_t>() ||
        !readText(doc["master"], state.master) || !readText(doc["source"], state.source) ||
        !readText(doc["session"], state.session) || !readText(doc["effect"], state.effect)) {
        reply(client, state, 400, "invalid");
        return;
    }
    state.sequence = doc["sequence"].as<uint32_t>();
#if IGNORE_WEB_EFFECT_CHANGES == 1
    reply(client, state, 403, "ignored");
    return;
#endif
    if (fxBroadcastEnabled.load()) {
        reply(client, state, 409, "master-conflict");
        return;
    }
    if (inbox.active() && client.remoteIP() != controlIp) {
        reply(client, state, 409, "master-conflict");
        return;
    }
    const auto order = inbox.inspect(state);
    if (order == mesh::Order::Conflict || order == mesh::Order::Stale) {
        reply(client, state, 409, order == mesh::Order::Stale ? "stale" : "conflict");
        return;
    }
    if (order == mesh::Order::Duplicate) {
        const bool applied = meshAppliedTicket.load(std::memory_order_acquire) == inbox.ticket();
        reply(client, state, applied ? 200 : 202, applied ? "applied" : "queued");
        return;
    }
    const uint16_t index = fxRegistry.findEffectIndex(state.effect);
    if (fxRegistry.getEffectId(index) != state.effect) {
        reply(client, state, 422, "unknown-effect");
        return;
    }
    if (++nextTicket == 0) ++nextTicket;
    const FxActionMessage msg{MESH_FX_UPDATE, index, nextTicket};
    if (xQueueSend(fxQueue, &msg, 0) != pdPASS) {
        reply(client, state, 503, "busy");
        return;
    }
    inbox.commit(state, nextTicket);
    recordMasterControl(client.remoteIP(), state.source);
    reply(client, state, 202, "queued");
}
