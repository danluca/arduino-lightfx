// Copyright (c) 2024 by Dan Luca. All rights reserved. 
//  

#include <Arduino.h>
#include "ledstate.h"

/**
 * Implementation of the analogWrite as when using WiFi in the project, the linker doesn't find the implementation of
 * this function defined in {@code ~\.platformio\packages\framework-arduinopico\variants\arduino_nano_connect\nina_pins.h}
 * @param pin the pin to write to, as the NinaPin wrapper object
 * @param value the value to PWM write to the pin (with
 */
void analogWrite(NinaPin pin, const int value) {
    analogWrite(pin.get(), value);
}

/**
 * Implementation of the pinMode as when using WiFi in the project, the linker doesn't find the implementation of
 * this function defined in {@code ~\.platformio\packages\framework-arduinopico\variants\arduino_nano_connect\nina_pins.h}
 * @param pin the pin to change mode for, as NinaPin wrapper object
 * @param mode the mode of the pin
 */
void pinMode(NinaPin pin, PinMode mode) {
    pinMode(pin.get(), mode);
}

/**
 * Setup the on-board status LED
 */
void setupStateLED() {
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);
    updateStateLED(0, 0, 0);    //black, turned off
}
/**
 * Controls the on-board status LED
 * @param colorCode
 */
void updateStateLED(uint32_t colorCode) {
    uint8_t r = (colorCode >> 16) & 0xFF;
    uint8_t g = (colorCode >>  8) & 0xFF;
    uint8_t b = (colorCode >>  0) & 0xFF;
    updateStateLED(r, g, b);
}

/**
 * Controls the onboard LED using individual values for R, G, B
 * @param red red value
 * @param green green value
 * @param blue blue value
 */
void updateStateLED(uint8_t red, uint8_t green, uint8_t blue) {
    analogWrite(LEDR, 255 - red);
    analogWrite(LEDG, 255 - green);
    analogWrite(LEDB, 255 - blue);
}

