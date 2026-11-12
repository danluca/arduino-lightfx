//
// Copyright by Dan Luca. All rights reserved
//
#pragma once

#include <cstddef>
#include <cstdint>

// These are supported operating limits, not preallocated buffer capacities.
/** Maximum number of LED strip pixels supported for runtime buffer allocation. */
constexpr uint16_t MAX_SUPPORTED_NUM_PIXELS = 1024;
/** Maximum animation frame buffer size supported. */
constexpr uint16_t MAX_SUPPORTED_FRAME_SIZE = 256;

/**
 * Supported LED driver chipsets (corresponds to FastLED controller types).
 */
enum class LedChipset : uint8_t {
    WS2811,     ///< WS2811 protocol (e.g. 12V addressable strip)
    WS2812B     ///< WS2812B protocol (e.g. 5V integrated NeoPixel strip)
};

/**
 * LED color channel byte ordering (matches FastLED EOrder).
 */
enum class LedColorOrder : uint8_t {
    RGB,
    RBG,
    GRB,
    GBR,
    BRG,
    BGR
};

/**
 * Hardware and network configuration profile for a specific physical board.
 */
struct BoardConfiguration {
    const char *uid;            ///< Unique 64-bit board identifier hex string, or nullptr for safe fallback
    const char *deviceName;     ///< Device / mDNS host name
    uint16_t numPixels;         ///< Number of physical LEDs on the primary strip
    uint16_t frameSize;         ///< Base frame size for animation patterns
    uint16_t mv3_3;             ///< Measured 3.3V power rail in millivolts for ADC calibration
    uint16_t vccDivR4;          ///< High-side resistor value (ohms) in the VCC voltage divider
    uint16_t vccDivR5;          ///< Low-side resistor value (ohms) in the VCC voltage divider
    uint8_t ip[4];              ///< Static IPv4 address octets, or {0,0,0,0} for DHCP
    uint8_t brightness;         ///< Default initial brightness (0-255)
    uint8_t maxBrightness;      ///< Maximum allowed brightness ceiling (0-255)
    LedChipset ledChipset;      ///< LED driver chipset type
    LedColorOrder colorOrder;   ///< LED color channel byte ordering

    /**
     * Checks if this board configuration uses DHCP for IPv4 address assignment.
     * @return true if all IP octets are 0, false if a static IP is configured
     */
    [[nodiscard]] bool useDhcp() const noexcept {
        return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
    }
};

/**
 * Returns the currently active runtime board configuration profile.
 * @return reference to the active board configuration
 */
const BoardConfiguration &activeBoardConfiguration();

/**
 * Returns the safe fallback board configuration with conservative defaults.
 * @return reference to the safe fallback board configuration
 */
const BoardConfiguration &safeBoardConfiguration();

/**
 * Retrieves the list of statically known board configuration profiles.
 * @param[out] count reference to receive the number of known configurations
 * @return pointer to the array of known board configurations
 */
const BoardConfiguration *knownBoardConfigurations(size_t &count);

/**
 * Looks up a board configuration matching the provided board UID string (case-insensitive).
 * @param uid board unique identifier string (hex)
 * @return pointer to matching configuration, or nullptr if not found
 */
const BoardConfiguration *findBoardConfiguration(const char *uid);

/**
 * Validates that configuration parameters are within safe physical and operational limits.
 * @param configuration board configuration to validate
 * @return true if valid and safe to use, false otherwise
 */
bool isValidBoardConfiguration(const BoardConfiguration &configuration);

/**
 * Clamps requested brightness to the board's configured maximum brightness ceiling.
 * @param configuration board configuration containing maxBrightness limit
 * @param requested requested brightness value (0-255)
 * @return brightness clamped to maxBrightness
 */
uint8_t limitBrightness(const BoardConfiguration &configuration, uint8_t requested);

/**
 * Selects and activates the board configuration matching the specified UID.
 * Falls back to safeBoardConfiguration() if the UID is not found or fails validation.
 * @param uid unique board identifier string
 * @return true if a matching valid configuration was selected, false if fell back to safe config
 */
bool selectBoardConfiguration(const char *uid);