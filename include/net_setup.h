//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_NET_SETUP_H
#define LIGHTFX_NET_SETUP_H

#include <WiFiNINA.h>
#include <NTPClient.h>
#include <Arduino_LSM6DSOX.h>
#include <TimeLib.h>
#include "secrets.h"
#include "PaletteFactory.h"

#define CST_OFFSET_SECONDS (-21600)   //Central Standard Time - America/Chicago
#define CDT_OFFSET_SECONDS  (-18000)  //Central Daylight Time - America/Chicago

extern WiFiServer server;
extern bool isDST;

void setupStateLED();
void stateLED(CRGB color);

bool wifi_setup();
bool imu_setup();
bool time_setup();
void server_setup();
void webserver();
void wifi_loop();
void printWifiStatus();
void checkFirmwareVersion();
time_t curUnixTime();
bool ntp_sync();
uint8_t barSignalLevel(int32_t rssi);

#endif //LIGHTFX_NET_SETUP_H
