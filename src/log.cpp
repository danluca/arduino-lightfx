//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
#include "log.h"
#include "PicoLog.h"
#include <FastLED.h>
#include "util.h"

void log_setup() {
#ifdef LOGGING_ENABLED
    if (!Serial) {
        Serial.begin(115200); // initialize serial communication - note the rate parameter is likely ignored, HW negotiates with the host PC the proper rate
        while (!Serial) {
            taskDelay(100);     //wait for the serial connection to be initiated by the PC
        }
    }
#endif
    Log.begin(&Serial, LogLevel::INFO);
    taskDelay(500);
    Log.info(F("========================================"));
}

