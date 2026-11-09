//
// Copyright by Dan Luca. All rights reserved
//
#pragma once

#include <cstddef>
#include <cstdint>

// These are supported operating limits, not preallocated buffer capacities.
constexpr uint16_t MAX_SUPPORTED_NUM_PIXELS = 1024;
constexpr uint16_t MAX_SUPPORTED_FRAME_SIZE = 256;

struct BoardConfiguration {
    const char *uid;
    const char *deviceName;
    uint16_t numPixels;
    uint16_t frameSize;
    uint16_t mv3_3;
    uint16_t vccDivR4;
    uint16_t vccDivR5;
    uint8_t ip[4];
    uint8_t brightness;
    uint8_t maxBrightness;
    [[nodiscard]] bool useDhcp() const noexcept {
        return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
    }
};

const BoardConfiguration &activeBoardConfiguration();
const BoardConfiguration &safeBoardConfiguration();
const BoardConfiguration *knownBoardConfigurations(size_t &count);
const BoardConfiguration *findBoardConfiguration(const char *uid);
bool isValidBoardConfiguration(const BoardConfiguration &configuration);
uint8_t limitBrightness(const BoardConfiguration &configuration, uint8_t requested);
bool selectBoardConfiguration(const char *uid);