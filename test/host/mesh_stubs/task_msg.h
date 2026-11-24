#pragma once
#include <cstdint>
#include <vector>
enum FxAction { MESH_FX_UPDATE, MESH_FX_RESTORE };
struct FxActionMessage { FxAction action; uint32_t data; uint32_t meshTicket = 0; };
inline int fxQueue = 1;
inline bool testQueueFull = false;
inline std::vector<FxActionMessage> testMessages;
inline constexpr int pdPASS = 1;
inline int xQueueSend(int, const FxActionMessage *message, int) {
    if (testQueueFull) return 0;
    testMessages.push_back(*message);
    return pdPASS;
}
