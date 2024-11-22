///////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2023 (c) Dan Luca
///////////////////////////////////////////////////////////////////////////////////////////////////
// Collection of light strip effects with ability to be configured through Wi-Fi
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <SchedulerExt.h>
#include "filesystem.h"
#include "net_setup.h"
#include "efx_setup.h"
#include "sysinfo.h"
#include "diag.h"
#include "broadcast.h"
#include "FxSchedule.h"
#include "ledstate.h"
#include "log.h"
#include "mic.h"

TaskDef fxTasks {fx_setup, fx_run, 3072, "Fx", 4, CORE_0};
TaskDef micTasks {mic_setup, mic_run, 896, "Mic", 4, CORE_0};
//ThreadTasks diagTasks {diag_setup, diag_run, 1792, "Diag"};

/**
 * Setup LED strip and global data structures - executed once
 */
void setup() {
    delay(2000);    //safety delay
    log_setup();
    setupStateLED();

    stateLED(CLR_SETUP_IN_PROGRESS);    //Setup in progress

    sysInfo = new SysInfo();    //system information object built once per run
    fsSetup();

    readSysInfo();
    secElement_setup();

    Scheduler.startTask(&fxTasks);
    Scheduler.startTask(&micTasks);

    stateLED(CLR_SETUP_IN_PROGRESS);    //Setup in progress
	bool bSetupOk = wifi_setup();
    bSetupOk = bSetupOk && timeSetup();
    stateLED(bSetupOk ? CLR_ALL_OK : CLR_SETUP_ERROR);

    setupAlarmSchedule();

    diagSetup();

    sysInfo->fillBoardId();

    watchdogSetup();

    broadcastSetup();

    Log.infoln(F("Main Setup completed. System status: %X"), sysInfo->getSysStatus());
    logSystemInfo();
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
