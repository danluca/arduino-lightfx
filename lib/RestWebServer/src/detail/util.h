// Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
//

#ifndef UTIL_H
#define UTIL_H
#include <Arduino.h>
#include "WiFiClient.h"

class Util {
  public:
    static String responseCodeToString(int code);
    static String getRandomHexString();
    static size_t readBytesWithTimeout(WiFiClient* client, char* buffer, size_t bufLength, int timeout_ms);
    static void delay(const uint32_t ms) { ::vTaskDelay(pdMS_TO_TICKS(ms)); };
};

#endif //UTIL_H
