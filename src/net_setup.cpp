//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved
//
#include "net_setup.h"
#include <WiFiNINA.h>
#include <queue.h>
#include <timers.h>
#include "config.h"
#include "sysinfo.h"
#include "timeutil.h"
#include "comms.h"
#include "ledstate.h"
#include "util.h"
#include "log.h"
#include "web_server.h"

using namespace colTheme;
constexpr auto ssid PROGMEM = WF_SSID;
constexpr auto pass PROGMEM = WF_PSW;
constexpr auto hostname PROGMEM = "lightfx-" DEVICE_NAME;

const CRGB CLR_ALL_OK = CRGB::Indigo;
const CRGB CLR_SETUP_IN_PROGRESS = CRGB::Orange;
const CRGB CLR_UPGRADE_PROGRESS = CRGB::Blue;
const CRGB CLR_SETUP_ERROR = CRGB::Red;
static uint16_t tmrWifiEnsure = 40;
static uint16_t tmrWifiTemp = 41;

UDP* mUdp = nullptr;  // mDNS UDP instance
MDNS* mdns = nullptr;

/**
 * Convenience to translate into number of bars the WiFi signal strength received from \code WiFi.RSSI() \endcode
 * <p>This Android article has been used for reference - https://android.stackexchange.com/questions/176320/rssi-range-for-wifi-icons </p>
 * @param rssi the RSSI value from WiFi system
 * @return number of bars as signal level - between 0 (no signal, connection likely lost) through 4 bars (strong signal)
 */
uint8_t barSignalLevel(const int32_t rssi) {
    static constexpr uint8_t numLevels = 5;
    static constexpr int16_t minRSSI = -100;
    static constexpr int16_t maxRSSI = -55;
    if (rssi <= minRSSI)
        return 0;
    if (rssi >= maxRSSI)
        return numLevels - 1;
    constexpr float inRange = maxRSSI - minRSSI;
    constexpr float outRange = numLevels - 1;
    return (uint8_t) ((float) (rssi - minRSSI) * outRange / inRange);
}

void stateLed(const CRGB& clr) {
    updateStateLED(clr.as_uint32_t());
}

bool wifi_connect() {
    //static IP address - such that we can have a known location for config page
    WiFi.config({IP_ADDR}, {IP_DNS}, {IP_GW}, {IP_SUBNET});
    WiFi.setHostname(hostname);
    Log.info(F("Connecting to WiFI '%s'"), ssid);  // print the network name (SSID);
    // attempt to connect to WiFi network:
    uint attCount = 0;
    uint8_t wifiStatus = WiFi.status();
    while (wifiStatus != WL_CONNECTED) {
        if (attCount > 60)
            stateLed(CLR_SETUP_ERROR);
        Log.info(F("Attempting to connect..."));

        // Connect to WPA/WPA2 network
        wifiStatus = WiFi.begin(ssid, pass);
        // wait 10 seconds for connection to succeed:
        taskDelay(10000);
        attCount++;
    }
    const bool result = wifiStatus == WL_CONNECTED;
    if (result) {
        sysInfo->setSysStatus(SYS_STATUS_WIFI);
        if (const int resPing = WiFi.ping(sysInfo->refGatewayIpAddress()); resPing >= 0)
            Log.info(F("Gateway ping successful: %d ms"), resPing);
        else
            Log.warn(F("Failed pinging the gateway - will retry later"));
        printSuccessfulWifiStatus();  // you're connected now, so print out the status
    }

    // setup mDNS - to resolve this board's address as 'lightfx-dev.local' or 'lightfx-fx01.local'
    String mdnsSvcName(hostname);
    mdnsSvcName.concat("-webserver._http");
    mUdp = new WiFiUDP();
    mdns = new MDNS(*mUdp);
    mdns->begin();
    MDNS::Status mdnsStatus = mdns->serviceInsert(MDNS::Service::Builder()
        .withName(mdnsSvcName)
        .withPort(80)
        .withProtocol(MDNS::Service::Protocol::TCP)
        .withTXT(MDNS::Service::TXT::Builder()
            .add("model", "NanoConnect RP2040")
            .build())
        .build()
    );
    (void)mdnsStatus;
    log_info(F("mDNS adding service status: %d (%s)"), svcStatus, MDNS::toString(svcStatus));
    mdnsStatus = mdns->start({IP_ADDR}, hostname);
    (void)mdnsStatus;
    log_info(F("mDNS start status: %d (%s)"), svcStatus, MDNS::toString(svcStatus));
    return result;
}

void enqueueWifiEnsure(TimerHandle_t xTimer) {
    constexpr CommAction action = WIFI_ENSURE;
    if (const BaseType_t qResult = xQueueSend(webQueue, &action, 0); qResult != pdTRUE)
        Log.error(F("Error sending WIFI_ENSURE message to Web queue for timer %d [%s] - error %ld"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
}

void enqueueWifiTempRead(TimerHandle_t xTimer) {
    constexpr CommAction action = WIFI_TEMP;
    if (const BaseType_t qResult = xQueueSend(webQueue, &action, 0); qResult != pdTRUE)
        Log.error(F("Error sending WIFI_TEMP message to Web queue for timer %d [%s] - error %ld"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
}

bool wifi_setup() {
    // check for the WiFi module:
    if (WiFi.status() == WL_NO_MODULE) {
        Log.warn(F("Communication with WiFi module failed!"));
        // don't continue - terminate thread?
        vTaskSuspend(nullptr);
        //while (true) vTaskYield();
    }
    checkFirmwareVersion();

    //enable low power mode - web server is not the primary function of this module
    WiFi.lowPowerMode();

    const bool connStatus = wifi_connect();

    // create timer to re-check WiFi and ensure connectivity - every 7 minutes
    const TimerHandle_t thWifiEnsure = xTimerCreate("wifiEnsure", pdMS_TO_TICKS(7*60*1000), pdTRUE, &tmrWifiEnsure, enqueueWifiEnsure);
    if (thWifiEnsure == nullptr)
        Log.error(F("Cannot create wifiEnsure timer - Ignored. There is NO wifi re-check scheduled"));
    else if (xTimerStart(thWifiEnsure, 0) != pdPASS)
        Log.error(F("Cannot start the wifiEnsure timer - Ignored."));
    //create timer to take WiFi temperature - every 33s, 1s off from the diagnostic thread temp read, to avoid overlap
    const TimerHandle_t thWifiTempRead = xTimerCreate("wifiTempRead", pdMS_TO_TICKS(33*1000), pdTRUE, &tmrWifiTemp, enqueueWifiTempRead);
    if (thWifiTempRead == nullptr)
        Log.error(F("Cannot create wifiTempRead timer - Ignored. There is NO wifi temperature read scheduled"));
    else if (xTimerStart(thWifiTempRead, 0) != pdPASS)
        Log.error(F("Cannot start the wifiTempRead timer - Ignored."));

    return connStatus;
}

/**
 * WiFi connection check
 * @return true if all is ok, false if connection unusable
 */
bool wifi_check() {
    if (WiFi.status() != WL_CONNECTED) {
        sysInfo->resetSysStatus(SYS_STATUS_WIFI);
        Log.warn(F("WiFi Connection lost"));
        return false;
    }
    const int gwPingTime = WiFi.ping(sysInfo->refGatewayIpAddress(), 64);
    const int32_t rssi = WiFi.RSSI();
    const uint8_t wifiBars = barSignalLevel(rssi);
    if ((gwPingTime < 0) || (wifiBars < 3)) {
        sysInfo->resetSysStatus(SYS_STATUS_WIFI);
        //we either cannot ping the router or the signal strength is 2 bars and under - reconnect for a better signal
        Log.warn(F("Ping test failed (%d) or signal strength low (%hhu bars), WiFi Connection unusable"), rssi, wifiBars);
        return false;
    }
    sysInfo->setSysStatus(SYS_STATUS_WIFI);
    Log.info(F("WiFi Ok - Gateway ping %d ms, RSSI %d (%hhu bars)"), gwPingTime, rssi, wifiBars);
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
    web::server.stop();
    Udp.stop();
    mdns->stop();
    delete mdns;
    delete mUdp;
    WiFi.disconnect();
    WiFi.end();     //without this, the re-connected wifi has closed socket clients
    taskDelay(2000);    //let disconnect state settle
    if (wifi_connect())
        stateLed(CLR_ALL_OK);
    //NVIC_SystemReset();
}

void wifi_ensure() {
    if (!wifi_check()) {
        stateLed(CLR_SETUP_ERROR);
        Log.warn(F("WiFi connection unusable/lost - reconnecting..."));
        wifi_reconnect();
        web::server_setup();
    }
    if (sysInfo->isSysStatus(SYS_STATUS_WIFI))
        postTimeSetupCheck();
    Log.info(F("System status: %#hX"), sysInfo->getSysStatus());
}

void printSuccessfulWifiStatus() {
    sysInfo->setWiFiInfo(WiFi);
    // print the SSID of the network you're attached to:
    Log.info(F("Connected to SSID: %s"), sysInfo->getSSID().c_str());

    // print your board's IP address:
    Log.info(F("IP Address: %s"), sysInfo->getIpAddress().c_str());

    // print your board's MAC address
    Log.info(F("MAC Address %s"), sysInfo->getMacAddress().c_str());

    // print the received signal strength:
    int32_t rssi = WiFi.RSSI();
    Log.info(F("Signal strength (RSSI) %d dBm; %hhu bars"), rssi, barSignalLevel(rssi));

    // print where to go in a browser:
    Log.info(F("Home page available at http://%s"), sysInfo->getIpAddress().c_str());
}

void checkFirmwareVersion() {
    const String fv = nina::WiFiClass::firmwareVersion();
    Log.info(F("WiFi firmware version %s"), fv.c_str());
    if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
        Log.warn(F("Please upgrade the WiFi firmware to %s"), WIFI_FIRMWARE_LATEST_VERSION);
    }
}
