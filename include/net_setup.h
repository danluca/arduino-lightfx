//
// Copyright 2023,2024,2025 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_NET_SETUP_H
#define LIGHTFX_NET_SETUP_H

#include "PaletteFactory.h"
#include "secrets.h"

extern QueueHandle_t webQueue;

bool wifi_setup();
void wifi_ensure();

void printSuccessfulWifiStatus();
void checkFirmwareVersion();
uint8_t barSignalLevel(int32_t rssi);



#endif //LIGHTFX_NET_SETUP_H
