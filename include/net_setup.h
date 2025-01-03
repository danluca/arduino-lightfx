//
// Copyright 2023,2024 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_NET_SETUP_H
#define LIGHTFX_NET_SETUP_H

#include <WiFiNINA.h>
#include "PaletteFactory.h"
#include "secrets.h"
//#include "util.h"
//#include <mutex>

extern WiFiServer server;
extern const CRGB CLR_ALL_OK;
extern const CRGB CLR_SETUP_IN_PROGRESS;
extern const CRGB CLR_SETUP_ERROR;
extern QueueHandle_t webQueue;

bool wifi_setup();
void wifi_ensure();

void server_setup();
void webserver();
void printSuccessfulWifiStatus();
void checkFirmwareVersion();
uint8_t barSignalLevel(int32_t rssi);

#endif //LIGHTFX_NET_SETUP_H
