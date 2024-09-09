//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
#include "log.h"
#ifndef DISABLE_LOGGING
#include <mbed.h>
#include <FastLED.h>

time_t logTimeOffset = 0;
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

    // Total time
    const time_t msecs =  millis() + logTimeOffset;
    const unsigned long secs =  msecs / MSECS_PER_SEC;

    // Time in components
    const unsigned long MilliSeconds =  msecs % MSECS_PER_SEC;
    const unsigned long Seconds =  secs  % SECS_PER_MIN ;
    const unsigned long Minutes = (secs  / SECS_PER_MIN) % SECS_PER_MIN;
    const unsigned long Hours = (secs  % SECS_PER_DAY) / SECS_PER_HOUR;

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

#ifndef DISABLE_LOGGING
void printRGB(Print* const out, const CRGB rgb) {
    uint32_t numClr = (uint32_t)rgb & 0xFFFFFF; //conversion to uint32 uses 0xFF for the alpha channel - we're not interested in the alpha channel
    out->print("0x");
    int8_t padding = 5 - Logging::countSignificantNibbles(numClr); //3 bytes, 6 digits 0 padded, accounting for at least one digit being printed
    while (padding-- > 0)
        out->print('0');
    out->print(numClr, HEX);
}
#endif

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
            printRGB(_logOutput, clr);
            break;
        }
        case 'R': {
            CRGBSet *set = va_arg(*args, CRGBSet*);
            _logOutput->print('['); _logOutput->print(set->size()); _logOutput->print(']');
            _logOutput->print('{');
            for (const auto &clr:*set) {
                printRGB(_logOutput, clr);
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
        case 'v': {
            uint64_t value = va_arg(*args, uint64_t);
            _logOutput->print(value, DEC);
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

