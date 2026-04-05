// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//
#include <Arduino.h>
#include <FastLED.h>

#include "sysinfo_internal.h"

namespace {

constexpr CRGB kClrAllOk = CRGB::Indigo;
constexpr CRGB kClrSetupInProgress = CRGB::Green;
constexpr CRGB kClrSetupError = CRGB::Red;

} // namespace

/**
 * Set-up the on-board status LED
 */
void SysInfo::setupStateLED() {
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    updateBoardLED(CRGB::Black);
}

/**
 * Controls the on-board status LED
 * @param colorCode
 */
void SysInfo::updateBoardLED(const uint32_t colorCode) {
    updateBoardLED(CRGB(colorCode));
}

/**
 * Controls the onboard LED using individual values for R, G, B
 * On RPI based boards (Pico 2W, Plasma, etc), the LED(s) are simple GPIO-controlled LED.
 * On Plasma 2350 W, the RGB LED is on GPIO 16, 17, 18
 * On RPi Pico 2 W, the status LED is on GPIO 15
 * @param rgb RGB value
 */
void SysInfo::updateBoardLED(const CRGB rgb) {
    analogWrite(PIN_LED_R, 255 - rgb.red);
    analogWrite(PIN_LED_G, 255 - rgb.green);
    analogWrite(PIN_LED_B, 255 - rgb.blue);
}

/**
 * Adjusts the LED state (color, illumination style) in response to the overall system's state
 */
void SysInfo::updateStatusLED() const {
    const bool isOk = isSysStatus(SysStatus::Wifi | SysStatus::Ntp | SysStatus::Filesystem | SysStatus::Diag);
    const CRGB colorCode = isOk ? kClrAllOk : !isSysStatus(SysStatus::Setup0 | SysStatus::Setup1) ? kClrSetupInProgress : kClrSetupError;
    updateBoardLED(colorCode);
}

/**
 * Flash status LED for as long as both cores are in setup mode
 */
void state_led_begin() {
    while (!sysInfo->isSysStatus(SysStatus::Setup0 | SysStatus::Setup1)) {
        SysInfo::updateBoardLED(CRGB::Black);
        taskDelay(640);
        SysInfo::updateBoardLED(kClrSetupInProgress);
        taskDelay(640);
    }
}

/**
 * Update the color of the status LED consistent with the system's state
 */
void state_led_update() {
    sysInfo->updateStatusLED();
}
