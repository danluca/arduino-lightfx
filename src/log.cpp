//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
#include "log.h"
#ifndef DISABLE_LOGGING
#include <mbed.h>
#include <FastLED.h>
#endif

void log_setup() {
#ifndef DISABLE_LOGGING
  if (!Serial) {
    Serial.begin(115200);      // initialize serial communication
    while(!Serial) {}
    Log.begin(LOG_LEVEL_INFO, &Serial, true);
    Log.infoln("========================================");
    Log.setPrefix(logPrefix);
    Log.setAdditionalFormatting(logExtraFormats);
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

void printThread(Print *_logOutput, int logLevel) {
#ifndef DISABLE_LOGGING
    char buf[20];
    snprintf(buf, 20, "[%s] ", rtos::ThisThread::get_name());
    _logOutput->print(buf);
#endif
}

void logPrefix(Print *_logOutput, int logLevel) {
#ifndef DISABLE_LOGGING
    printTimestamp(_logOutput);
    printThread(_logOutput, logLevel);
//    printLogLevel(_logOutput, logLevel);
#endif
}

/**
 * Additional format character handling in excess of what <code>Logging::printFormat(const char, va_list*)</code> provides
 * @param _logOutput the logger object
 * @param fmt format character, not null
 * @param args the arguments list - next in the list is to be formatted into the logger as specified by the given format character
 */
void logExtraFormats(Print *_logOutput, const char fmt, va_list *args) {
#ifndef DISABLE_LOGGING
    switch (fmt) {
        case 'r': {
            CRGB clr = va_arg(*args, CRGB);
            uint32_t numClr = (uint32_t)clr & 0xFFFFFF; //conversion to uint32 uses 0xFF for the alpha channel - we're not interested in the alpha channel
            _logOutput->print("0x");
            if (Logging::countSignificantNibbles(numClr)%2)
                _logOutput->print('0');
            _logOutput->print(numClr, HEX);
            break;
        }
        case 'R': {
            CRGBSet *set = va_arg(*args, CRGBSet*);
            _logOutput->print('['); _logOutput->print(set->size()); _logOutput->print(']');
            _logOutput->print('{');
            for (const auto &clr:*set) {
                _logOutput->print("0x");
                uint32_t numClr = (uint32_t)clr & 0xFFFFFF; //mask out the alpha channel (the fourth byte, MSB) as it is set to 0xFF
                if (Logging::countSignificantNibbles(numClr)%2)
                    _logOutput->print('0');
                _logOutput->print(numClr, HEX);
                _logOutput->print(", ");
            }
            _logOutput->print('}');
            break;
        }
        case 'y': {
            time_t time = va_arg(*args, time_t);
            char timeStr[20];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&time));
            _logOutput->print(timeStr);
            break;
        }
        default:
            _logOutput->print("n/s");
    }
#endif
}

void writeChar(char c) {
#ifndef DISABLE_LOGGING
  Serial.write(c);
#endif
}

