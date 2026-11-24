#include <cassert>
#include <iostream>
#include "mesh_http.h"
#include "task_msg.h"
#include "comms.h"

template<size_t N, size_t M>
static void copyText(char (&dest)[N], const char (&text)[M]) {
    static_assert(M <= N);
    std::memcpy(dest, text, M);
}

static mesh::State state(uint32_t sequence = 1) {
    mesh::State value;
    copyText(value.master, "board-id");
    copyText(value.source, "Outdoor \"tree\"");
    copyText(value.session, "boot-session");
    copyText(value.effect, "FXB");
    value.sequence = sequence;
    return value;
}

static WebClient request(const mesh::State &value) {
    WebClient client;
    assert(meshEncodeState(value, client.req.payload));
    handleMeshState(client);
    return client;
}

int main() {
    auto value = state();
    mesh::Inbox ordering;
    assert(ordering.inspect(value) == mesh::Order::New);
    ordering.commit(value, 1);
    assert(ordering.inspect(value) == mesh::Order::Duplicate);
    auto changed = value;
    changed.sequence = 0;
    assert(ordering.inspect(changed) == mesh::Order::Stale);
    changed = value;
    copyText(changed.effect, "FXA");
    assert(ordering.inspect(changed) == mesh::Order::Conflict);
    changed = value;
    copyText(changed.master, "other-board");
    assert(ordering.inspect(changed) == mesh::Order::Conflict);
    changed = value;
    copyText(changed.session, "rebooted");
    assert(ordering.inspect(changed) == mesh::Order::New);
    ordering.commit(changed, 2);
    assert(ordering.inspect(value) == mesh::Order::Stale); // Delayed retries from the prior boot.
    value.sequence = 0xffffffffu;
    ordering.commit(value, 2);
    value.sequence = 0;
    assert(ordering.inspect(value) == mesh::Order::New);

    mesh::Delivery delivery;
    delivery.start(1);
    assert(delivery.due(0));
    delivery.attempted(0);
    assert(!delivery.due(999));
    assert(delivery.due(1000));
    delivery.attempted(1000);
    delivery.attempted(2000);
    assert(!delivery.due(3000)); // Bounded even if every acknowledgment is lost.
    delivery.start(2);
    assert(delivery.due(60000)); // Periodic state heals exhausted retries.
    delivery.applied(0xfffffff0u);
    assert(delivery.confirmed(0x20u));
    assert(!delivery.confirmed(0xfffffff0u + mesh::leaseMs));
    delivery.rejected();
    assert(!delivery.confirmed(0x20u));

    value = state();
#if IGNORE_WEB_EFFECT_CHANGES == 1
    assert(request(value).code == 403);
    assert(testMessages.empty());
#else
    testQueueFull = true;
    assert(request(value).code == 503);
    assert(!meshControlActive());
    testQueueFull = false;
    auto queued = request(value);
    assert(queued.code == 202);
    assert(testMessages.size() == 1);
    assert(!meshAppliedAck(queued.response, value));
    assert(request(value).code == 202);
    assert(testMessages.size() == 1); // Duplicate retries never enqueue twice.
    meshAppliedTicket.store(testMessages.back().meshTicket);
    auto applied = request(value); // Lost response recovers by repeating the same request.
    assert(applied.code == 200);
    assert(meshAppliedAck(applied.response, value));
    auto mismatch = value;
    mismatch.sequence++;
    assert(!meshAppliedAck(applied.response, mismatch));
    copyText(mismatch.effect, "FXA");
    assert(!meshAppliedAck(applied.response, mismatch));
    assert(!meshAppliedAck(String(mesh::maxBodyBytes + 1, 'x'), value));
    JsonDocument doc;
    describeMeshControl(doc.to<JsonObject>());
    assert(doc["masterBoard"] == value.source);

    // Wrong IP, competing master, stale sequence and invalid effect cannot renew the lease.
    WebClient other;
    meshEncodeState(value, other.req.payload);
    other.ip.value = 2;
    handleMeshState(other);
    assert(other.code == 409);
    mismatch = value;
    mismatch.sequence = 0;
    assert(request(mismatch).code == 409);
    mismatch.sequence = 2;
    copyText(mismatch.effect, "missing");
    assert(request(mismatch).code == 422);
    WebClient malformed;
    malformed.req.payload = "{}";
    handleMeshState(malformed);
    assert(malformed.code == 400);
    malformed.req.payload = String(mesh::maxStateBytes + 1, 'x');
    handleMeshState(malformed);
    assert(malformed.code == 400);
    malformed.req.payload = "{\"protocol\":1,\"type\":\"state\",\"sequence\":true}";
    handleMeshState(malformed);
    assert(malformed.code == 400);

    // Heartbeats with an unchanged effect carry a new sequence and refresh the lease.
    testMillis = 60000;
    value.sequence++;
    assert(request(value).code == 202);
    const auto count = testMessages.size();
    testMillis = 60000 + mesh::leaseMs - 1;
    checkMasterControlTimeout();
    assert(testMessages.size() == count);
    testMillis++;
    testQueueFull = true;
    checkMasterControlTimeout();
    assert(meshControlActive()); // Restoration is retried when the queue is full.
    testQueueFull = false;
    checkMasterControlTimeout();
    assert(!meshControlActive());
    assert(testMessages.back().action == MESH_FX_RESTORE);
    assert(request(value).code == 202); // Reconnect starts a fresh local snapshot.
    clearMasterControl();
    fxBroadcastEnabled.store(true);
    assert(request(value).code == 409);
    fxBroadcastEnabled.store(false);

    // Legacy masters retain the longer timeout during a rolling firmware upgrade.
    testMillis = 0;
    recordMasterControl(IPAddress{1}, "legacy");
    testMillis = mesh::leaseMs;
    checkMasterControlTimeout();
    assert(meshControlActive());
    testMillis = 20 * 60000;
    checkMasterControlTimeout();
    assert(!meshControlActive());

    assert(request(state(100)).code == 202);
    testQueueFull = true;
    assert(!meshPrepareMaster());
    assert(meshControlActive());
    testQueueFull = false;
    assert(meshPrepareMaster());
    assert(!meshControlActive());
    assert(testMessages.back().action == MESH_FX_RESTORE);
#endif
    std::cout << "Mesh HTTP tests passed\n";
}
