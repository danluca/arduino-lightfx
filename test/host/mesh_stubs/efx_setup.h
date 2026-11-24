#pragma once
#include "Arduino.h"
struct TestRegistry {
    // Lookup failure deliberately returns zero, matching production's API.
    uint16_t findEffectIndex(const char *id) { return String(id) == "FXB" ? 1 : 0; }
    String getEffectId(uint16_t index) { return index == 1 ? "FXB" : "FXA"; }
};
inline TestRegistry fxRegistry;
