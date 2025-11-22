//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved
//
#include "net_setup.h"
#include <WiFi.h>
#include "config.h"
#include "sysinfo.h"
#include "timeutil.h"
#include "comms.h"
#include "util.h"
#include "log.h"
#include "stringutils.h"
#include "web_server.h"
#if MDNS_ENABLED==1
#include <LEAmDNS.h>
#endif

// using namespace colTheme;
constexpr auto ssid PROGMEM = WF_SSID;
constexpr auto pass PROGMEM = WF_PSW;
constexpr auto hostname PROGMEM = "lightfx-" DEVICE_NAME;

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
    return static_cast<uint8_t>(static_cast<float>(rssi - minRSSI) * outRange / inRange);
}

bool wifi_connect() {
    //static IP address - such that we can have a known location for config page
    // WiFi.config({IP_ADDR});
    WiFi.setHostname(hostname);
    log_info(F("Connecting to WiFI '%s'"), ssid);  // print the network name (SSID);
    // attempt to connect to WiFi network:
    // WiFi.setTimeout(10000);     // default timeout is 15 seconds - see WiFiClass.h
    uint attCount = 0;
    uint8_t wifiStatus = WiFi.begin(ssid, pass);
    while (wifiStatus != WL_CONNECTED) {
        log_info(F("Attempting to connect Wi-Fi..."));

        // Connect to WPA/WPA2 network
        // wait 2 seconds for connection to succeed:
        taskDelay(2500);
        wifiStatus = WiFi.begin(ssid, pass);
        attCount++;
    }
    const bool result = wifiStatus == WL_CONNECTED;
    if (result) {
        sysInfo->setSysStatus(SYS_STATUS_WIFI);
        if (const int resPing = WiFi.ping(sysInfo->refGatewayIpAddress()); resPing >= 0)
            log_info(F("Connected to WiFi after %d tries. Gateway ping successful: %d ms"), attCount, resPing);
        else
            log_warn(F("Connected to WiFi after %d tries. Failed pinging the gateway (ping result %d) - will retry later"), attCount, resPing);
        printSuccessfulWifiStatus();  // you're connected now, so print out the status
    }
#if MDNS_ENABLED==1
    // setup mDNS - to resolve this board's address as 'lightfx-dev.local' or 'lightfx-fx01.local'
    String dnsHostname(hostname);
    dnsHostname.toLowerCase();
    String webSvcName(dnsHostname);
    webSvcName.concat(F("-webserver._http"));
    String lightfxSvcName(dnsHostname);
    lightfxSvcName.concat(F("._lucasfx"));
    const bool mdnsStatus = MDNS.begin(dnsHostname);
    (void)mdnsStatus;
    log_info(F("mDNS start status: %d (%s)"), mdnsStatus, StringUtils::asString(mdnsStatus));
    MDNS.addService(webSvcName, "_tcp", 80);
    MDNS.addServiceTxt(webSvcName, "_tcp", "info", "Pimoroni Plasma 2350W Lucas LightFX");
    // MDNS.addServiceTxt(webSvcName, "_tcp", "name", dnsHostname);
    // MDNS.addServiceTxt(webSvcName, "_tcp", "model", "Plasma 2350W");
    log_info(F("mDNS added web service %s"), webSvcName.c_str());
    // MDNS.addService(lightfxSvcName, "_tcp", 80);
    // MDNS.addServiceTxt(lightfxSvcName, "_tcp", "info", "Pimoroni Plasma 2350W Lucas LightFX");
    // MDNS.addServiceTxt(lightfxSvcName, "_tcp", "name", dnsHostname);
    // MDNS.addServiceTxt(lightfxSvcName, "_tcp", "model", "Plasma 2350W");
    // log_info(F("mDNS added custom lucasfx service %s"), lightfxSvcName.c_str());
    MDNS.announce();
#endif

    return result;
}

bool wifi_setup() {
    // check for the WiFi module:
    if (WiFi.status() == WL_NO_MODULE) {
        log_warn(F("Communication with WiFi module failed!"));
        // don't continue - terminate thread?
        vTaskSuspend(nullptr);
        //while (true) vTaskYield();
    }
    checkFirmwareVersion();
    //enable low-power mode - web server is not the primary function of this module
    WiFi.defaultLowPowerMode();

    const bool connStatus = wifi_connect();

    return connStatus;
}

/**
 * WiFi connection check
 * @return true if all is ok, false if connection unusable
 */
bool wifi_check() {
    if (WiFi.status() != WL_CONNECTED) {
        sysInfo->resetSysStatus(SYS_STATUS_WIFI);
        log_warn(F("WiFi Connection lost"));
        return false;
    }
    const int gwPingTime = WiFi.ping(sysInfo->refGatewayIpAddress(), 64);
    const int32_t rssi = WiFi.RSSI();
    const uint8_t wifiBars = barSignalLevel(rssi);
    if ((gwPingTime < 0) || (rssi < -73)) {
        sysInfo->resetSysStatus(SYS_STATUS_WIFI);
        //we either cannot ping the router or the signal strength is 2 bars and under - reconnect for a better signal
        log_warn(F("Ping test failed (%d) or signal strength low (%d dbM, %hhu bars), WiFi Connection unusable"), gwPingTime, rssi, wifiBars);
        return false;
    }
    sysInfo->setSysStatus(SYS_STATUS_WIFI);
    log_info(F("WiFi Ok - Gateway ping %d ms, RSSI %d (%hhu bars)"), gwPingTime, rssi, wifiBars);
    return true;
}

/**
 * Similar with wifi_connect, but with some preamble cleanup
 * Watch out: https://github.com/arduino/nina-fw/issues/63 - after WiFi reconnect, the server seems to stop working (returns disconnected clients?)
 * Should we invoke a board reset instead? (NVIC_SystemReset)
 */
void wifi_reconnect() {
    sysInfo->resetSysStatus(SYS_STATUS_WIFI);
    web::server.stop();
    timeService.end();
    delete ntpUDP;
#if MDNS_ENABLED==1
    MDNS.close();
#endif

    WiFi.disconnect();
    WiFi.end();     //without this, the re-connected wifi has closed socket clients
    log_info(F("Web services stopped, UDP clients terminated, WiFi disconnected"));
    taskDelay(2000);    //let disconnect state settle
    wifi_connect();
    //NVIC_SystemReset();
}

void wifi_ensure() {
    if (!wifi_check()) {
        log_warn(F("WiFi connection unusable/lost - reconnecting..."));
        wifi_reconnect();
        web::server_setup();
    }
    if (sysInfo->isSysStatus(SYS_STATUS_WIFI))
        postTimeSetupCheck();
    log_info(F("System status: %#hX"), sysInfo->getSysStatus());
}

void printSuccessfulWifiStatus() {
    sysInfo->setWiFiInfo(WiFi);
    // print the SSID of the network you're attached to:
    log_info(F("Connected to SSID: %s"), sysInfo->getSSID().c_str());

    // print your board's IP address:
    log_info(F("IP Address: %s"), sysInfo->getIpAddress().c_str());

    // print your board's MAC address
    log_info(F("MAC Address %s"), sysInfo->getMacAddress().c_str());

    // print the received signal strength:
    int32_t rssi = WiFi.RSSI();
    log_info(F("Signal strength (RSSI) %d dBm; %hhu bars"), rssi, barSignalLevel(rssi));

    // print where to go in a browser:
    log_info(F("Home page available at http://%s"), sysInfo->getIpAddress().c_str());
}

void checkFirmwareVersion() {
    const String fv = ::WiFiClass::firmwareVersion();
    log_info(F("WiFi firmware version %s"), fv.c_str());
    if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
        log_warn(F("Please upgrade the WiFi firmware to %s"), WIFI_FIRMWARE_LATEST_VERSION);
    }
}
