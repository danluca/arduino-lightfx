#include "log.h"

void log_setup() {
#ifndef DISABLE_LOGGING
  if (!Serial) {
    Serial.begin(115200);      // initialize serial communication
    while(!Serial) {}
    Log.begin(LOG_LEVEL_INFO, &Serial, true);
    Log.infoln("========================================");
    Log.setPrefix(logPrefix);
  }
#endif
}

void printTimestamp(Print* _logOutput) {
#ifndef DISABLE_LOGGING
    // Division constants
    const unsigned long MSECS_PER_SEC       = 1000;
    const unsigned long SECS_PER_MIN        = 60;
    const unsigned long SECS_PER_HOUR       = 3600;
    const unsigned long SECS_PER_DAY        = 86400;

    // Total time
    const unsigned long msecs               =  millis();
    const unsigned long secs                =  msecs / MSECS_PER_SEC;

    // Time in components
    const unsigned long MilliSeconds        =  msecs % MSECS_PER_SEC;
    const unsigned long Seconds             =  secs  % SECS_PER_MIN ;
    const unsigned long Minutes             = (secs  / SECS_PER_MIN) % SECS_PER_MIN;
    const unsigned long Hours               = (secs  % SECS_PER_DAY) / SECS_PER_HOUR;

    // Time as string
    char timestamp[20];
    sprintf(timestamp, "%02lu:%02lu:%02lu.%03lu ", Hours, Minutes, Seconds, MilliSeconds);
    _logOutput->print(timestamp);
#endif
}

void printLogLevel(Print* _logOutput, int logLevel) {
#ifndef DISABLE_LOGGING
    /// Show log description based on log level
    switch (logLevel)
    {
        default:
        case 0:_logOutput->print("SLN " ); break;
        case 1:_logOutput->print("FTL "  ); break;
        case 2:_logOutput->print("ERR "  ); break;
        case 3:_logOutput->print("WRN "); break;
        case 4:_logOutput->print("INF "   ); break;
        case 5:_logOutput->print("TRC "  ); break;
        case 6:_logOutput->print("VRB "); break;
    }
#endif
}

void logPrefix(Print *_logOutput, int logLevel) {
#ifndef DISABLE_LOGGING
    printTimestamp(_logOutput);
//    printLogLevel(_logOutput, logLevel);
#endif
}


void writeChar(char c) {
#ifndef DISABLE_LOGGING
  Serial.write(c);
#endif
}

