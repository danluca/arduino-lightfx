///////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2023 (c) Dan Luca
///////////////////////////////////////////////////////////////////////////////////////////////////
// Collection of light strip effects with ability to be configured through Wi-Fi
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <SchedulerExt.h>
#include "net_setup.h"
#include "efx_setup.h"
#include "log.h"
#include "sysinfo.h"
#include "diag.h"
#include "broadcast.h"

FxTasks fxTasks {fx_setup, fx_run};
FxTasks micTasks {mic_setup, mic_run};
ThreadTasks fxTasks {fx_setup, fx_run, 3072, "Fx"};
ThreadTasks micTasks {mic_setup, mic_run, 896, "Mic"};
//ThreadTasks diagTasks {diag_setup, diag_run, 1792, "Diag"};

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
    setupStateLED();

    stateLED(CLR_SETUP_IN_PROGRESS);    //Setup in progress

    sysInfo = new SysInfo();    //system information object built once per run
    fsInit();

    readSysInfo();
    secElement_setup();

    Scheduler.startTask(&fxTasks);
    Scheduler.startTask(&micTasks);

    stateLED(CLR_SETUP_IN_PROGRESS);    //Setup in progress
	bool bSetupOk = wifi_setup();
    bSetupOk = bSetupOk && time_setup();
    stateLED(bSetupOk ? CLR_ALL_OK : CLR_SETUP_ERROR);

    setupAlarmSchedule();

    diag_events_setup();

    sysInfo->fillBoardId();

    watchdogSetup();

    postWiFiSetupEvent();

    Log.infoln(F("Main Setup completed. System status: %X"), sysInfo->getSysStatus());
    logSystemInfo();
    //Scheduler.startTask(&micTasks, 1024);
    auto *micTask = new TaskJob("MIC", mic_run, mic_setup, 1024);
    micTask->setCoreAffinity(CORE_0);
    Scheduler.startTask(micTask);
}

/**
 * Main loop - runs the WebServer
 */
void loop() {
    wifi_loop();
    alarm_loop();
//    watchdogPing();   //the main functionality is in Fx thread, we can afford not having web server available
    yield();
}

//Second core tasks
//void setup1() {
//
//}
//
//void loop1() {
//
//}
