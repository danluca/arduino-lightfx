//
// Copyright 2023,2024,2025 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_NET_SETUP_H
#define LIGHTFX_NET_SETUP_H

#include "PaletteFactory.h"
#include "secrets.h"

bool wifi_setup();
void wifi_ensure();
void wifi_temp();

void printSuccessfulWifiStatus();
void checkFirmwareVersion();
uint8_t barSignalLevel(int32_t rssi);



#endif //LIGHTFX_NET_SETUP_H
