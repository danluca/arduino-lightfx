// Copyright (c) 2025 by Dan Luca. All rights reserved.
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
};

#endif //UTIL_H
