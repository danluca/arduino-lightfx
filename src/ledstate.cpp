// Copyright (c) 2024 by Dan Luca. All rights reserved. 
//  
#include <Arduino.h>
#include "ledstate.h"

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

