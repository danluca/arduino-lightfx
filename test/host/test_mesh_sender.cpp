#include <cassert>
#include <vector>
#include <memory>
#include <iostream>
#include "mesh_http.h"
#include "efx_setup.h"
#include "comms.h"
#include "log.h"

#include "sysinfo.h"
#include "broadcast_client.h"
#include "mesh_sender.h"

BroadcastRecipients fxBroadcastRecipients;

static int resultCode = 202;
static bool validAck = true;
static unsigned sends = 0;
static std::vector<uint8_t> sentPeers;
static uint32_t sentSequence = 0;
static String sentSession;

int meshExchange(const IPAddress &ip, const String &body, String &response) {
    ++sends;
    sentPeers.push_back(ip.octets[3]);
    JsonDocument doc;
    assert(!deserializeJson(doc, body));
    assert(doc["protocol"] == 1);
    assert(doc["type"] == "state");
    assert(doc["effect"] == "FXB");
    sentSequence = doc["sequence"].as<uint32_t>();
    sentSession = doc["session"].as<String>();
    doc["type"] = "ack";
    doc["status"] = resultCode == 200 ? "applied" : "queued";
    if (!validAck) doc["session"] = "different-session";
    serializeJson(doc, response);
    return resultCode;
}

int main() {
    fxBroadcastRecipients.push(std::make_unique<BroadcastClient>(IPAddress{}, static_cast<uint8_t>(fxBroadcastRecipients.size() + 10)));
    fxBroadcastRecipients.push(std::make_unique<BroadcastClient>(IPAddress{}, static_cast<uint8_t>(fxBroadcastRecipients.size() + 10)));
    serviceMeshClients(); // Empty/uninitialized recipient list is harmless.
    assert(sends == 0);
    meshPublishEffect(1);
    fxBroadcastEnabled.store(true);
    serviceMeshClients();
    assert(sends == 1 && sentSequence == 1);
    const String firstSession = sentSession;
    serviceMeshClients();
    assert(sends == 2); // One request per loop, regardless of the number of peers.
    serviceMeshClients();
    assert(sends == 2);
    testMillis = 999;
    serviceMeshClients();
    assert(sends == 2);
    testMillis = 1000;
    resultCode = 200;
    serviceMeshClients();
    serviceMeshClients();
    assert(sends == 4);
    assert((sentPeers == std::vector<uint8_t>{10, 11, 10, 11}));
    for (auto &peer : fxBroadcastRecipients) assert(peer->delivery.confirmed(testMillis));
    serviceMeshClients();
    assert(sends == 4);

    testMillis = mesh::announcementMs - 1;
    serviceMeshClients();
    assert(sends == 4);
    testMillis++;
    resultCode = -1;
    serviceMeshClients();
    serviceMeshClients();
    assert(sends == 6 && sentSequence == 2);
    testMillis += mesh::retryMs;
    serviceMeshClients();
    serviceMeshClients();
    testMillis += mesh::retryMs;
    serviceMeshClients();
    serviceMeshClients();
    assert(sends == 10);
    testMillis += mesh::retryMs;
    serviceMeshClients();
    assert(sends == 10); // Exhausted retries cannot create a busy retry loop.

    testMillis = 120000;
    resultCode = 200;
    validAck = false;
    serviceMeshClients();
    serviceMeshClients();
    assert(sentSequence == 3);
    testMillis = 181001;
    assert(!fxBroadcastRecipients.front()->delivery.confirmed(testMillis));
    validAck = true;
    serviceMeshClients();
    serviceMeshClients();
    assert(sentSequence == 4);
    for (auto &peer : fxBroadcastRecipients) assert(peer->delivery.confirmed(testMillis));

    fxBroadcastEnabled.store(false);
    serviceMeshClients();
    const auto before = sends;
    testMillis += 60000;
    serviceMeshClients();
    assert(sends == before);
    fxBroadcastEnabled.store(true);
    serviceMeshClients();
    assert(sends == before + 1);
    assert(sentSession != firstSession && sentSequence == 1);
    sysInfo->wifi = false;
    serviceMeshClients();
    assert(sends == before + 1);
    sysInfo->wifi = true;
    serviceMeshClients();
    assert(sends == before + 2);
    std::cout << "Mesh sender tests passed\n";
}
