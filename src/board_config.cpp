//
// Copyright by Dan Luca. All rights reserved
//
#include "board_config.h"

namespace {
    constexpr BoardConfiguration safeConfiguration {
        .uid = nullptr, .deviceName = "TBD", .numPixels = 32, .frameSize = 12, .mv3_3 = 3300, .vccDivR4 = 20000, .vccDivR5 = 3300,
        .ip = {0,0,0,0}, .brightness = 160, .maxBrightness = 190
    };

    // Board settings are intentionally kept in the application image so every unit
    // can use the same firmware. Add a row when a board UID is recorded.
    constexpr BoardConfiguration boardConfigurations[] {
        {.uid = "E034C6063295681A", .deviceName = "FX02-Office", .numPixels = 170, .frameSize = 76, .mv3_3 = 3262, .vccDivR4 = 19890, .vccDivR5 = 3302,
         .ip={192, 168, 0, 12}, .brightness = 224, .maxBrightness = 255},
        {.uid = "5034C60632956818", .deviceName = "Dev-Office", .numPixels = 170, .frameSize = 76, .mv3_3 = 3262, .vccDivR4 = 21950, .vccDivR5 = 3304,
            .ip = {192, 168, 0, 10}, .brightness = 224, .maxBrightness = 255},
        {.uid = "unknown-FX01", .deviceName = "FX01-House", .numPixels = 320, .frameSize = 68, .mv3_3 = 3242, .vccDivR4 = 21800, .vccDivR5 = 3305,
            .ip = {192, 168, 0, 11}, .brightness = 224, .maxBrightness = 255}
    };

    const BoardConfiguration *selectedConfiguration = &safeConfiguration;

    bool uidEqualsIgnoreCase(const char *left, const char *right) {
        if (left == nullptr || right == nullptr)
            return false;
        while (*left != '\0' && *right != '\0') {
            char a = *left++;
            char b = *right++;
            if (a >= 'a' && a <= 'f')
                a = static_cast<char>(a - 'a' + 'A');
            if (b >= 'a' && b <= 'f')
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
    if (configuration.frameSize <= 10 || configuration.frameSize >= configuration.numPixels || configuration.frameSize > MAX_SUPPORTED_FRAME_SIZE)
        return false;
    if (configuration.mv3_3 < 2500 || configuration.mv3_3 > 3600)
        return false;
    if (configuration.vccDivR4 < 1000 || configuration.vccDivR4 > 30000 ||
        configuration.vccDivR5 < 1000 || configuration.vccDivR5 > 30000)
        return false;
    if (configuration.brightness > configuration.maxBrightness)
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