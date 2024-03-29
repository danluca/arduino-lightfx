//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_NET_SETUP_H
#define LIGHTFX_NET_SETUP_H

#include <WiFiNINA.h>
#include <Arduino_LSM6DSOX.h>
#include "PaletteFactory.h"
#include "util.h"
#include "secrets.h"

extern WiFiServer server;

bool wifi_setup();

bool imu_setup();
void server_setup();
void webserver();
void wifi_loop();
void printSuccessfulWifiStatus();
void checkFirmwareVersion();
uint8_t barSignalLevel(int32_t rssi);

#endif //LIGHTFX_NET_SETUP_H
