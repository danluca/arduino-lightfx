#pragma once
#include <cstdint>
#include <string>
using String = std::string;
struct IPAddress {
    unsigned value = 0;
    uint8_t octets[4]{};
    uint8_t &operator[](size_t index) { return octets[index]; }
    String toString() const { return std::to_string(octets[3]); }
    bool operator!=(IPAddress other) const { return value != other.value; }
};
inline uint32_t testMillis = 0;
inline uint32_t millis() { return testMillis; }
#define F(value) value
