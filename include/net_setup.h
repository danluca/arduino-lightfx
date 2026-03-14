//
// Copyright 2023,2024,2025,2026 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_NET_SETUP_H
#define LIGHTFX_NET_SETUP_H

#include <Arduino.h>
#include <vector>
#include "secrets.h"

struct DiscoveredBoard {
    String hostname;
    IPAddress ip;
    uint16_t port;
    String serviceName;
    unsigned long lastSeen;
};

bool wifi_setup();
void wifi_ensure();
void wifi_temp();

void printSuccessfulWifiStatus();
void checkFirmwareVersion();
uint8_t barSignalLevel(int32_t rssi);



#endif //LIGHTFX_NET_SETUP_H
