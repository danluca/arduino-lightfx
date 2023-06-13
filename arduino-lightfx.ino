#include <FastLED.h>
#include <Scheduler.h>
#include "config.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2023 (c) Dan Luca
///////////////////////////////////////////////////////////////////////////////////////////////////
// Collection of light strip effects with ability to be configured through WiFi
// 
///////////////////////////////////////////////////////////////////////////////////////////////////

//flips these to enable/disable logging to USB serial port
// #define DEBUG
#undef DEBUG

// This is an array of leds.  One item for each led in your strip.
CRGB leds[NUM_PIXELS];

void setupStateLED() {
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  stateLED(CRGB::Black);
}

void stateLED(CRGB color) {
  analogWrite(LEDR, 255-color.r);
  analogWrite(LEDG, 255-color.g);
  analogWrite(LEDB, 255-color.b);
}

/**
 * Setup LED strip and global data structures - executed once
 */
void setup() {
  delay(1000);    //safety delay
  setupStateLED();
  stateLED(CRGB::OrangeRed);

  log_setup();
  wifi_setup();

  fx_setup();

  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_PIXELS).setCorrection(TypicalSMD5050).setTemperature(Tungsten100W);
  FastLED.setBrightness( BRIGHTNESS );

  //start the web server/fx in a separate thread - turns out the JSON library crashes quite often if ran in non-primary thread
  // Scheduler.startLoop(wifi_loop);
  Scheduler.startLoop(fx_run);
  stateLED(CRGB::Indigo);
}

/**
 * Main Light Effects loop
 */
void loop() {
  // fx_run();
  wifi_loop();
}

