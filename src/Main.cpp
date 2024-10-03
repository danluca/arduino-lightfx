///////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2023 (c) Dan Luca
///////////////////////////////////////////////////////////////////////////////////////////////////
// Collection of light strip effects with ability to be configured through Wi-Fi
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "efx_setup.h"
#include "log.h"
#include "net_setup.h"
#include "FxSchedule.h"
#include <SchedulerExt.h>
#include "sysinfo.h"

ThreadTasks fxTasks {fx_setup, fx_run, 3072, "Fx"};
ThreadTasks micTasks {mic_setup, mic_run, 1024, "Mic"};

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
    delay(2000);    //safety delay
    log_setup();
    adc_setup();
    setupStateLED();

    stateLED(CLR_SETUP_IN_PROGRESS);    //Setup in progress

    sysInfo = new SysInfo();    //system information object built once per run
    fsInit();

    readSysInfo();
    imu_setup();
    secElement_setup();

    Scheduler.startTask(&fxTasks);
    Scheduler.startTask(&micTasks);

    stateLED(CLR_SETUP_IN_PROGRESS);    //Setup in progress
	bool bSetupOk = wifi_setup();
    bSetupOk = bSetupOk && time_setup();
    stateLED(bSetupOk ? CLR_ALL_OK : CLR_SETUP_ERROR);

    setupAlarmSchedule();

    sysInfo->fillBoardId();
    Log.infoln(F("System status: %X"), sysInfo->getSysStatus());
    logSystemInfo();

    watchdogSetup();

    //start the web server/fx in a separate thread - turns out the JSON library crashes if not given enough stack size
    // Scheduler.startLoop(wifi_loop, 2048);
}

/**
 * Main loop - runs the WebServer
 */
void loop() {
    wifi_loop();
    alarm_loop();
    watchdogPing();
    yield();
}

