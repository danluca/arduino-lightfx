//
// Copyright by Dan Luca. All rights reserved
//
#include "board_config.h"
#include <cstdio>
#include <cstring>

#ifndef LIGHTFX_TREE_BOARD_UID
#define LIGHTFX_TREE_BOARD_UID "SET_TREE_UID"
#endif

namespace {
    /** Fallback configuration applied when no board UID matches or configuration is invalid. */
    constexpr BoardConfiguration safeConfiguration {
        .uid = nullptr, .deviceName = "TBD", .numPixels = 32, .frameSize = 16, .mv3_3 = 3300, .vccDivR4 = 20000, .vccDivR5 = 4700,
        .ip = {0,0,0,0}, .brightness = 160, .maxBrightness = 190, .ledChipset = LedChipset::WS2811, .colorOrder = LedColorOrder::RGB
    };

    // Board settings are intentionally kept in the application image so every unit
    // can use the same firmware. Add a row when a board UID is recorded.
    /** Statically registered hardware board profiles. */
    constexpr BoardConfiguration boardConfigurations[] {
        {.uid = "DEB8609F71125183", .deviceName = "Xmas2350", .numPixels = 66, .frameSize = 22, .mv3_3 = 3317, .vccDivR4 = 22000, .vccDivR5 = 4700,
         .ip = {0,0,0,0}, .brightness = 224, .maxBrightness = 255, .ledChipset = LedChipset::WS2812B, .colorOrder = LedColorOrder::BGR},
        {.uid = LIGHTFX_TREE_BOARD_UID, .deviceName = "FXPine", .numPixels = 1000, .frameSize = 75, .mv3_3 = 3317, .vccDivR4 = 22000, .vccDivR5 = 4700,
         .ip = {0,0,0,0}, .brightness = 224, .maxBrightness = 255, .ledChipset = LedChipset::WS2811, .colorOrder = LedColorOrder::RGB}
    };

    const BoardConfiguration *selectedConfiguration = &safeConfiguration;

    /**
     * Performs a case-insensitive string equality comparison for hex UID strings.
     * @param left first null-terminated string
     * @param right second null-terminated string
     * @return true if strings match case-insensitively, false otherwise
     */
    bool uidEqualsIgnoreCase(const char *left, const char *right) {
        if (left == nullptr || right == nullptr || *left == '\0' || *right == '\0')
            return false;
        while (*left != '\0' && *right != '\0') {
            char a = *left++;
            char b = *right++;
            if (a >= 'a' && a <= 'z')
                a = static_cast<char>(a - 'a' + 'A');
            if (b >= 'a' && b <= 'z')
                b = static_cast<char>(b - 'a' + 'A');
            if (a != b)
                return false;
        }
        return *left == *right;
    }
}

const BoardConfiguration &activeBoardConfiguration() {
    return *selectedConfiguration;
}

const BoardConfiguration &safeBoardConfiguration() {
    return safeConfiguration;
}

const BoardConfiguration *knownBoardConfigurations(size_t &count) {
    count = sizeof(boardConfigurations) / sizeof(boardConfigurations[0]);
    return boardConfigurations;
}

const BoardConfiguration *findBoardConfiguration(const char *uid) {
    size_t count = 0;
    const BoardConfiguration *configurations = knownBoardConfigurations(count);
    for (size_t i = 0; i < count; ++i) {
        if (uidEqualsIgnoreCase(uid, configurations[i].uid))
            return &configurations[i];
    }
    return nullptr;
}

bool isValidBoardConfiguration(const BoardConfiguration &configuration) {
    if (configuration.numPixels == 0 || configuration.numPixels > MAX_SUPPORTED_NUM_PIXELS)
        return false;
    if (configuration.frameSize <= 14 || configuration.frameSize >= configuration.numPixels || configuration.frameSize > MAX_SUPPORTED_FRAME_SIZE)
        return false;
    if (configuration.mv3_3 < 2500 || configuration.mv3_3 > 3600)
        return false;
    if (configuration.vccDivR4 < 1000 || configuration.vccDivR4 > 30000 ||
        configuration.vccDivR5 < 1000 || configuration.vccDivR5 > 30000)
        return false;
    if (configuration.brightness > configuration.maxBrightness)
        return false;
    if (configuration.ledChipset != LedChipset::WS2811 && configuration.ledChipset != LedChipset::WS2812B)
        return false;
    //these checks below are specific to the LED strips we use - so far BTF WS2811 (12V) and WS2812B (5V)
    if (configuration.ledChipset == LedChipset::WS2811 && configuration.colorOrder != LedColorOrder::RGB)
        return false;
    if (configuration.ledChipset == LedChipset::WS2812B && configuration.colorOrder != LedColorOrder::BGR)
        return false;
    return true;
}

uint8_t limitBrightness(const BoardConfiguration &configuration, const uint8_t requested) {
    return requested > configuration.maxBrightness ? configuration.maxBrightness : requested;
}

bool selectBoardConfiguration(const char *uid) {
    const BoardConfiguration *configuration = findBoardConfiguration(uid);
    if (configuration == nullptr || !isValidBoardConfiguration(*configuration)) {
        selectedConfiguration = &safeConfiguration;
        return false;
    }
        selectedConfiguration = configuration;
        return true;
}