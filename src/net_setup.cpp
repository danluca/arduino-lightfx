//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
#include "net_setup.h"
#include "config.h"
#include "sysinfo.h"
#include "timeutil.h"
#include "broadcast.h"
#include "ledstate.h"
#include "log.h"

using namespace colTheme;
const char ssid[] PROGMEM = WF_SSID;
const char pass[] PROGMEM = WF_PSW;
const char hostname[] PROGMEM = "Arduino-RP2040-" DEVICE_NAME;

const CRGB CLR_ALL_OK = CRGB::Indigo;
const CRGB CLR_SETUP_IN_PROGRESS = CRGB::Orange;
const CRGB CLR_SETUP_ERROR = CRGB::Red;

/**
 * Convenience to translate into number of bars the WiFi signal strength received from {@code WiFi.RSSI()}
 * <p>This Android article has been used for reference - https://android.stackexchange.com/questions/176320/rssi-range-for-wifi-icons </p>
 * @param rssi the RSSI value from WiFi system
 * @return number of bars as signal level - between 0 (no signal, connection likely lost) through 4 bars (strong signal)
 */
uint8_t barSignalLevel(int32_t rssi) {
    const static uint8_t numLevels = 5;
    const static int16_t minRSSI = -100;
    const static int16_t maxRSSI = -55;
    if (rssi <= minRSSI)
        return 0;
    else if (rssi >= maxRSSI)
        return numLevels - 1;
    float inRange = maxRSSI - minRSSI;
    float outRange = numLevels - 1;
    return (uint8_t) ((float) (rssi - minRSSI) * outRange / inRange);
}

void stateLed(const CRGB& clr) {
    updateStateLED(clr.as_uint32_t());
}

bool wifi_connect() {
    //static IP address - such that we can have a known location for config page
    WiFi.config({IP_ADDR}, {IP_DNS}, {IP_GW}, {IP_SUBNET});
    WiFi.setHostname(hostname);
    Log.infoln("Connecting to WiFI '%s'", ssid);  // print the network name (SSID);
    // attempt to connect to WiFi network:
    uint attCount = 0;
    uint8_t wifiStatus = WiFi.status();
    while (wifiStatus != WL_CONNECTED) {
        if (attCount > 60)
            stateLed(CLR_SETUP_ERROR);
        Log.infoln(F("Attempting to connect..."));

        // Connect to WPA/WPA2 network
        wifiStatus = WiFi.begin(ssid, pass);
        // wait 10 seconds for connection to succeed:
        delay(10000);
        attCount++;
    }
    bool result = wifiStatus == WL_CONNECTED;
    if (result) {
        sysInfo->setSysStatus(SYS_STATUS_WIFI);
        int resPing = WiFi.ping(WiFi.gatewayIP());
        if (resPing >= 0)
            Log.infoln(F("Gateway ping successful: %d ms"), resPing);
        else
            Log.warningln(F("Failed pinging the gateway - will retry later"));
    }
    server_setup();     // start the web server on port 80
    printSuccessfulWifiStatus();  // you're connected now, so print out the status

    return result;
}

bool wifi_setup() {
    // check for the WiFi module:
    if (WiFi.status() == WL_NO_MODULE) {
        Log.warningln(F("Communication with WiFi module failed!"));
        // don't continue - terminate thread?
        //rtos::ThisThread::terminate();
        while (true) yield();
    }
    checkFirmwareVersion();

    //enable low power mode - web server is not the primary function of this module
    //WiFi.lowPowerMode();

    return wifi_connect();
}

/**
 * WiFi connection check
 * @return true if all is ok, false if connection unusable
 */
bool wifi_check() {
    if (WiFi.status() != WL_CONNECTED) {
        sysInfo->resetSysStatus(SYS_STATUS_WIFI);
        Log.warningln(F("WiFi Connection lost"));
        return false;
    }
    int gwPingTime = WiFi.ping(WiFi.gatewayIP(), 64);
    int32_t rssi = WiFi.RSSI();
    uint8_t wifiBars = barSignalLevel(rssi);
    if ((gwPingTime < 0) || (wifiBars < 3)) {
        sysInfo->resetSysStatus(SYS_STATUS_WIFI);
        //we either cannot ping the router or the signal strength is 2 bars and under - reconnect for a better signal
        Log.warningln(F("Ping test failed (%d) or signal strength low (%d bars), WiFi Connection unusable"), rssi, wifiBars);
        return false;
    }
    sysInfo->setSysStatus(SYS_STATUS_WIFI);
    Log.infoln(F("WiFi Ok - Gateway ping %d ms, RSSI %d (%d bars)"), gwPingTime, rssi, wifiBars);
    return true;
}

/**
 * Similar with wifi_connect, but with some preamble cleanup
 * Watch out: https://github.com/arduino/nina-fw/issues/63 - after WiFi reconnect, the server seems to stop working (returns disconnected clients?)
 * Should we invoke a board reset instead? (NVIC_SystemReset)
 */
void wifi_reconnect() {
    sysInfo->resetSysStatus(SYS_STATUS_WIFI);
    stateLed(CLR_SETUP_IN_PROGRESS);
    if (WiFiClient client = server.available())
        client.stop();
    server.stop();
    Udp.stop();
    WiFi.disconnect();
    WiFi.end();     //without this, the re-connected wifi has closed socket clients
    delay(2000);    //let disconnect state settle
    if (wifi_connect()) {
        stateLed(CLR_ALL_OK);
        broadcastSetup();
    }
    //NVIC_SystemReset();
}

void wifi_loop() {
    EVERY_N_MINUTES(7) {
        if (!wifi_check()) {
            stateLed(CLR_SETUP_ERROR);
            Log.warningln(F("WiFi connection unusable/lost - reconnecting..."));
            wifi_reconnect();
        }
        if (sysInfo->isSysStatus(SYS_STATUS_WIFI))
            postTimeSetupCheck();
        Log.infoln(F("System status: %X"), sysInfo->getSysStatus());
    }
    webserver();
}

void printSuccessfulWifiStatus() {
    sysInfo->setWiFiInfo(WiFi);
    // print the SSID of the network you're attached to:
    Log.infoln(F("Connected to SSID: %s"), sysInfo->getSSID().c_str());

    // print your board's IP address:
    Log.infoln(F("IP Address: %s"), sysInfo->getIpAddress().c_str());

    // print your board's MAC address
    Log.infoln(F("MAC Address %s"), sysInfo->getMacAddress().c_str());

    // print the received signal strength:
#ifndef DISABLE_LOGGING
    int32_t rssi = WiFi.RSSI();
    Log.infoln(F("Signal strength (RSSI) %d dBm; %d bars"), rssi, barSignalLevel(rssi));
#endif

    // print where to go in a browser:
    Log.infoln(F("Home page available at http://%s"), sysInfo->getIpAddress().c_str());
}

void checkFirmwareVersion() {
    const String fv = ::WiFiClass::firmwareVersion();
    Log.infoln(F("WiFi firmware version %s"), fv.c_str());
    if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
        Log.warningln(F("Please upgrade the WiFi firmware to %s"), WIFI_FIRMWARE_LATEST_VERSION);
    }
}
