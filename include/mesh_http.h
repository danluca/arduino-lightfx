// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//

#pragma once
#include <atomic>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <RestWebServer.h>
#include "mesh_protocol.h"

// Only these atomics cross between the FX task and the web/comms task.
extern std::atomic<uint32_t> meshAppliedTicket;
void meshPublishEffect(uint16_t index);
bool meshPublishedEffect(uint16_t &index);

extern String masterBoardName;
void clearMasterControl();
bool meshControlActive();
bool meshVersionedControlActive();
bool meshPrepareMaster();
void recordMasterControl(const IPAddress &ip, const String &name); // legacy /fx
void checkMasterControlTimeout();
void describeMeshControl(JsonObject master);
void handleMeshState(WebClient &client);

// Shared encoding and strict acknowledgment matching for both HTTP client libraries.
bool meshEncodeState(const mesh::State &state, String &body);
bool meshAppliedAck(const String &body, const mesh::State &state);
// Board-specific synchronous adapter: one request, no application retry sleeps.
// Socket/driver waits can exceed the configured HTTP timeouts.
int meshExchange(const IPAddress &ip, const String &body, String &response);
