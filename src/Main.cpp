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

void adc_setup() {
    //disable ADC
    //hw_clear_bits(&adc_hw->cs, ADC_CS_EN_BITS);
    //enable ADC, including temp sensor
    adc_init();
    adc_set_temp_sensor_enabled(true);
    analogReadResolution(ADC_RESOLUTION);   //get us the higher resolution of the ADC
}

/**
 * Setup LED strip and global data structures - executed once
 */
void setup() {
    delay(1000);    //safety delay
    log_setup();
    adc_setup();
    setupStateLED();

    fsInit();

    imu_setup();
    secElement_setup();

    Scheduler.startLoop(&fxTasks, 3072);
    Scheduler.startLoop(&micTasks, 1024);

    stateLED(CRGB::OrangeRed);    //Setup in progress
    if (wifi_setup())
        stateLED(CRGB::Indigo);   //ready to show awesome light effects!
    if (!time_setup())
        stateLED(CRGB::Blue);

    Log.infoln(F("System status: %X"), getSysStatus());
    //start the web server/fx in a separate thread - turns out the JSON library crashes if not given enough stack size
    // Scheduler.startLoop(wifi_loop, 2048);
}

/**
 * Main loop - runs the WebServer
 */
void loop() {
    wifi_loop();
}

