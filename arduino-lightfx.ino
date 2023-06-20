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

  fx_setup();
  Scheduler.startLoop(fx_run, 1024);

  stateLED(CRGB::OrangeRed);    //wifi connect in progress
  bool wifiOk = wifi_setup();
  if (wifiOk)
    stateLED(CRGB::Indigo);   //ready to show awesome light effects!
  else
    stateLED(CRGB::Green);

  //start the web server/fx in a separate thread - turns out the JSON library crashes if not given enough stack size
  // Scheduler.startLoop(wifi_loop, 2048);
}

/**
 * Main loop - runs the WebServer
 */
void loop() {
  wifi_loop();
}

