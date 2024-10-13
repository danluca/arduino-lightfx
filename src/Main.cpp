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

ThreadTasks fxTasks {fx_setup, fx_run, 3072, "Fx"};
ThreadTasks micTasks {mic_setup, mic_run, 896, "Mic"};
ThreadTasks diagTasks {diag_setup, diag_run, 1792, "Diag"};

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
    sysInfo->resetSysStatus(0xFF);
    secElement_setup();

    Scheduler.startTask(&fxTasks);
    Scheduler.startTask(&micTasks);

    stateLED(CLR_SETUP_IN_PROGRESS);    //Setup in progress
	bool bSetupOk = wifi_setup();
    bSetupOk = bSetupOk && time_setup();
    stateLED(bSetupOk ? CLR_ALL_OK : CLR_SETUP_ERROR);

    Scheduler.startTask(&diagTasks);

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

