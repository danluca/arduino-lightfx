//
// Created by Dan on 06.21.2023.
//

#ifndef LIGHTFX_NET_SETUP_H
#define LIGHTFX_NET_SETUP_H

#include <WiFiNINA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <Arduino_LSM6DSOX.h>
#include <TimeLib.h>
#include "log.h"
#include "secrets.h"

#define CST_OFFSET_SECONDS -21600

extern WiFiServer server;

void setupStateLED();
void stateLED(CRGB color);

bool wifi_setup();
void webserver();
void wifi_loop();
void printWifiStatus();
void checkFirmwareVersion();
time_t curUnixTime();
bool ntp_sync();

#endif //LIGHTFX_NET_SETUP_H
