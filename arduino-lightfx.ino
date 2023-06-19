#include <FastLED.h>
#include <Scheduler.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2023 (c) Dan Luca
///////////////////////////////////////////////////////////////////////////////////////////////////
// Collection of light strip effects with ability to be configured through WiFi
// 
///////////////////////////////////////////////////////////////////////////////////////////////////

//flips these to enable/disable logging to USB serial port
// #define DEBUG
#undef DEBUG

/**
 * Setup LED strip and global data structures - executed once
 */
void setup() {
  delay(1000);    //safety delay

  log_setup();
  
  setupStateLED();

  stateLED(CRGB::OrangeRed);    //wifi connect in progress
  bool auxOk = wifi_setup();
  
  fx_setup();

  //start the web server/fx in a separate thread - turns out the JSON library crashes quite often if ran in non-primary thread
  // Scheduler.startLoop(wifi_loop);
  Scheduler.startLoop(fx_run);
  if (auxOk)
    stateLED(CRGB::Indigo);   //ready to show awesome light effects!
  else
    stateLED(CRGB::DarkRed);
}

/**
 * Main Light Effects loop
 */
void loop() {
  // fx_run();
  wifi_loop();
}

