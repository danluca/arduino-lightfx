///////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2023 (c) Dan Luca
///////////////////////////////////////////////////////////////////////////////////////////////////
// Collection of light strip effects with ability to be configured through Wi-Fi
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "efx_setup.h"
#include "log.h"
#include "net_setup.h"
#include <SchedulerExt.h>

ThreadTasks fxTasks {fx_setup, fx_run};
ThreadTasks micTasks {mic_setup, mic_run};

/**
 * Setup LED strip and global data structures - executed once
 */
void setup() {
    delay(1000);    //safety delay
    log_setup();

    setupStateLED();

    fsInit();

    Scheduler.startLoop(&fxTasks, 3072);
    Scheduler.startLoop(&micTasks, 1024);

    stateLED(CRGB::OrangeRed);    //Wi-Fi connect in progress
    bool wifiOk = wifi_setup();
    if (wifiOk)
        stateLED(CRGB::Indigo);   //ready to show awesome light effects!
    if (!imu_setup())
        stateLED(CRGB::Green);
    if (!time_setup())
        stateLED(CRGB::Blue);

    //start the web server/fx in a separate thread - turns out the JSON library crashes if not given enough stack size
    // Scheduler.startLoop(wifi_loop, 2048);
}

/**
 * Main loop - runs the WebServer
 */
void loop() {
    wifi_loop();
}

