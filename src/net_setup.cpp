//
// Copyright (c) by Dan Luca. All rights reserved
//
#include "net_setup.h"
#include <algorithm>
#include <WiFi.h>
#include "config.h"
#include "sysinfo.h"
#include "timeutil.h"
#include "comms.h"
#include "constants.hpp"
#include "util.h"
#include "log.h"
#include "stringutils.h"
#include "web_server.h"
#include "HealthMonitor.h"
#if MDNS_ENABLED==1
#include <LEAmDNS.h>
#endif

#define DEVICE_NAME_PREFIX "lightfx-"

// using namespace colTheme;
constexpr auto ssid = WF_SSID;
constexpr auto pass = WF_PSW;
constexpr auto hostname = DEVICE_NAME_PREFIX DEVICE_NAME;
constexpr auto service_type = "lucasfx";
constexpr auto service_protocol = "tcp";

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
    if (rssi <= minRSSI || rssi >= 0)
        return 0;
    if (rssi >= maxRSSI)
        return numLevels - 1;
    static constexpr float inRange = maxRSSI - minRSSI;
    static constexpr float outRange = numLevels - 1;
    return static_cast<uint8_t>(static_cast<float>(rssi - minRSSI) * outRange / inRange);
}

#if MDNS_ENABLED==1
static mutex_t discBoardsMutex;
static std::vector<MDNSResponder::hMDNSService> serviceHandles;
// storage for service queries
static std::vector<MDNSResponder::hMDNSServiceQuery> serviceQueries;
// Storage for discovered boards - by itself is not thread-safe
static std::vector<DiscoveredBoard> discoveredBoards;

/**
 * Callback for MDNS service query results - we're registering the new board into the discoveredBoards vector
 * We're mostly interested in callbacks that have enuServiceQueryAnswerType::ServiceQueryAnswerType_IP4Address bit set in the answer type
 * @param service the service information details
 * @param answer type of answer received - see MDNSResponder::AnswerType for options
 * @param bEntryRegistered whether the service entry has been registered with MDNS (true) or deleted from (false)
 */
void serviceQueryCallback(const MDNSResponder::MDNSServiceInfo& service, MDNSResponder::AnswerType answer, bool bEntryRegistered) {
    if (!(static_cast<uint32_t>(answer) & static_cast<uint32_t>(MDNSResponder::AnswerType::IP4Address)))
        return;
    MDNSResponder::MDNSServiceInfo svcInfo = service;   //copy the service info to be able to call its methods; the passed in reference is const qualified and the member functions are not
    const char* svcName = svcInfo.serviceDomain();
    const char* hostname = svcInfo.hostDomainAvailable() ? svcInfo.hostDomain() : strNR;
    const uint16_t port = svcInfo.hostPortAvailable() ? svcInfo.hostPort() : 0;
    const IPAddress ip4_addr = svcInfo.IP4AddressAvailable() ? svcInfo.IP4Adresses().front() : IPAddress(0);

    log_info(F("mDNS discovered host %s at %s:%d for service %s. Answer type %ld, component set %d"), hostname, ip4_addr.toString().c_str(), port, svcName, answer, bEntryRegistered);

    if (!String(hostname).startsWith(DEVICE_NAME_PREFIX) || !bEntryRegistered) return;
    //TODO: extract service type from the full service name (split by '.', second element)

    // Check if board already in the list
    CoreMutex lock(&discBoardsMutex);   //we're likely modifying shared data - lock it
    bool found = false;
    for (auto& board : discoveredBoards) {
        if (board.hostname == hostname) {
            board.ip = ip4_addr;
            board.port = port;
            board.lastSeen = millis();
            found = true;
            break;
        }
    }

    if (!found) {
        const DiscoveredBoard board {.hostname = hostname, .ip = ip4_addr, .port = port, .serviceName = svcName, .lastSeen = millis()};
        discoveredBoards.push_back(board);
        log_info(F("Discovered lightfx board: %s at %s:%d"), hostname, ip4_addr.toString().c_str(), port);
    }
}

/**
 * When the board starts mDNS (e.g., claiming the name lightfx-dev.local), it must first "probe" the network to ensure no other device
 * is already using that name.
 * @param p_pcDomainName the name of the domain being probed
 * @param p_bProbeResult the result of the probe - true means the name is unique and has been successfully claimed;
 *    false means the name is already in use by another device
 */
void hostProbeCallback(const char *p_pcDomainName, bool p_bProbeResult) {
#if LOGGING_ENABLED == 1
    if (p_bProbeResult) {
        log_info(F("mDNS host - successfully claimed host domain %s"), p_pcDomainName ? p_pcDomainName : strNR);
    } else {
        log_error(F("mDNS host - failed to claim host domain %s"), p_pcDomainName ? p_pcDomainName : strNR);
    }
#endif
}

/**
 * The service probe callback is invoked when the board attempts to register a service with mDNS.
 * @param p_pcServiceName the name of the service being probed
 * @param p_hMDNSService the handle to the service being probed
 * @param p_bProbeResult the result of the probe - true means the name is unique and has been successfully claimed;
 *    false means the name is already in use by another device
 */
void hostServiceCallback(const char *p_pcServiceName, const MDNSResponder::hMDNSService p_hMDNSService, bool p_bProbeResult) {
#if LOGGING_ENABLED == 1
    if (p_bProbeResult) {
        log_info(F("mDNS service - successfully claimed service %s (handle: %p)"), p_pcServiceName ? p_pcServiceName : strNR, p_hMDNSService);
    } else {
        log_error(F("mDNS service - failed to claim service %s (handle: %p)"), p_pcServiceName ? p_pcServiceName : strNR, p_hMDNSService);
    }
#endif
}

#endif

/**
 * Blocking function until Wi-Fi connection succeeds.
 * @return true when connection succeeds; otherwise blocks
 */
bool wifi_connect() {
    uint8_t wifiStatus = WiFi.status();
    if (wifiStatus == WL_CONNECTED) return true;
    //static IP address - such that we can have a known location for config page
    // WiFi.config({IP_ADDR});
    WiFi.setHostname(hostname);
    log_info(F("Connecting to WiFI '%s' starting from status %hhu..."), ssid, wifiStatus);  // print the network name (SSID);
    // attempt to connect to WiFi network:
    WiFi.setTimeout(7500);     // default timeout is 15 seconds - see WiFiClass.h

    uint attCount = 1;
    while (wifiStatus != WL_CONNECTED) {
        const unsigned long startAttemptTime = millis();
        wifiStatus = WiFi.begin(ssid, pass);
        // Wait for connection with a 30-second timeout
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
            HealthMonitor::checkIn(HEALTH_CORE0);
            taskDelay(500);
        }
        if (wifiStatus = WiFi.status(); wifiStatus != WL_CONNECTED) {
            log_warn(F("WiFi connection attempt failed after %lu ms. Disconnecting and trying again"), millis() - startAttemptTime);
            WiFi.disconnect(); // Clear failed state
            attCount++;
        } else
            log_info(F("WiFi connected after %lu ms!"), millis() - startAttemptTime);
    }

    sysInfo->setSysStatus(SysStatus::Wifi);
    if (const int resPing = WiFi.ping(sysInfo->refGatewayIpAddress()); resPing >= 0)
        log_info(F("Connected to WiFi after %d tries. Gateway ping successful: %d ms"), attCount, resPing);
    else
        log_warn(F("Connected to WiFi after %d tries. Failed pinging the gateway (ping result %d) - will retry later"), attCount, resPing);
    printSuccessfulWifiStatus();  // we're connected now, so print out the status
#if MDNS_ENABLED==1
    // setup mDNS - to resolve this board's address as 'lightfx-dev.local' or 'lightfx-fx01.local'
    String dnsHostname(hostname);
    dnsHostname.toLowerCase();
    const bool mdnsStatus = MDNS.begin(dnsHostname);
    log_info(F("mDNS start status: %d (%s)"), mdnsStatus, StringUtils::asString(mdnsStatus));
    if (mdnsStatus) {
        // Add lucasfx service broadcasting
        const MDNSResponder::hMDNSService hFxSvc = MDNS.addService(dnsHostname.c_str(), service_type, service_protocol, 80);
        MDNS.addServiceTxt(hFxSvc, "info", "Pimoroni Plasma 2350W Lucas LightFX");
        MDNS.addServiceTxt(hFxSvc, "model", "Plasma 2350W");
        log_info(F("mDNS added custom lucasfx service %s"), dnsHostname.c_str());

        serviceHandles.reserve(4);      //reserve space for 4 service handles
        serviceQueries.reserve(4);      //reserve space for 4 service queries
        discoveredBoards.reserve(16);   //reserve space for 16 boards

        serviceHandles.push_back(hFxSvc);
        //install service queries for discovery of other boards
        if (const MDNSResponder::hMDNSServiceQuery hServiceQuery = MDNS.installServiceQuery(service_type, service_protocol, serviceQueryCallback); hServiceQuery)
            serviceQueries.push_back(hServiceQuery);
        else
            log_error("Error installing lucasfx service query");
        MDNS.setServiceProbeResultCallback(hFxSvc, hostServiceCallback);
        MDNS.setHostProbeResultCallback(hostProbeCallback);
    }
#endif

    return true;
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
    WiFi.noLowPowerMode();
    WiFi.mode(WIFI_STA);   //station mode - we're connecting to an existing WiFi network, not creating our own

    const bool connStatus = wifi_connect();

    return connStatus;
}

/**
 * WiFi connection check
 * @return true if all is ok, false if connection unusable
 */
bool wifi_check() {
    if (WiFi.status() != WL_CONNECTED) {
        sysInfo->resetSysStatus(SysStatus::Wifi);
        log_warn(F("WiFi Connection lost"));
        return false;
    }
    int gwPingTime = -1;
    uint8_t pingAttempts = 0;
    for (int i = 0; i < 4; i++) {
        HealthMonitor::checkIn(HEALTH_CORE0);
        gwPingTime = WiFi.ping(sysInfo->refGatewayIpAddress(), 128);
        pingAttempts++;
        if (gwPingTime >= 0)
            break;  // Gateway responsive, bail out
        taskDelay(100);  // Brief delay between attempts to avoid transient states
    }
    const int32_t rssi = WiFi.RSSI();
    const uint8_t wifiBars = barSignalLevel(rssi);
    if ((gwPingTime < 0) || (rssi < -75)) {
        sysInfo->resetSysStatus(SysStatus::Wifi);
        //we either cannot ping the router or the signal strength is 2 bars and under - reconnect for a better signal
        log_warn(F("Ping test to %s failed (%d) or signal strength low (%ld dbM, %hhu bars, %u tries), WiFi Connection unusable"),
            sysInfo->refGatewayIpAddress().toString().c_str(), gwPingTime, rssi, wifiBars, pingAttempts);
        return false;
    }
    sysInfo->setSysStatus(SysStatus::Wifi);
    log_info(F("WiFi Ok - Gateway %s ping %d ms, RSSI %ld (%hhu bars, %u tries)"), sysInfo->refGatewayIpAddress().toString().c_str(),
        gwPingTime, rssi, wifiBars, pingAttempts);
    return true;
}

/**
 * Similar with wifi_connect, but with some preamble cleanup
 */
void wifi_reconnect() {
    log_debug(F("wifi_reconnect: start"));
    sysInfo->resetSysStatus(SysStatus::Wifi);
    log_debug(F("wifi_reconnect: stopping web server"));
    web::server.stop();
    log_debug(F("wifi_reconnect: stopping time service"));
    timeService.end();
    delete ntpUDP;
    ntpUDP = nullptr;
#if MDNS_ENABLED==1
    for (const auto& query : serviceQueries)
        MDNS.removeServiceQuery(query);
    for (const auto& handle : serviceHandles)
        MDNS.removeService(handle);
    MDNS.removeQuery();
    MDNS.close();
#endif

    log_debug(F("wifi_reconnect: disconnecting WiFi"));
    WiFi.disconnect();
    WiFi.end();     //without this, the re-connected wifi has closed socket clients
    log_info(F("Web services stopped, UDP clients terminated, WiFi disconnected"));
    taskDelay(2000);    //let disconnect state settle
    wifi_connect();
#if MDNS_ENABLED==1
    MDNS.announce();
#endif
    //NVIC_SystemReset();
}

void wifi_ensure() {
    if (!wifi_check()) {
        log_warn(F("WiFi connection unusable/lost - reconnecting..."));
        wifi_reconnect();
        web::server_setup();
    }
    if (sysInfo->isSysStatus(SysStatus::Wifi))
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

#if MDNS_ENABLED==1

// Read-only access - returns a copy
std::vector<DiscoveredBoard> mdns_get_discovered_boards() {
    CoreMutex lock(&discBoardsMutex);
    return discoveredBoards;  // Copy elision/move will optimize this
}
/**
 * Trims the boards not seen in a while - call periodically from maintenance task
 */
void mdns_trim_boards() {
    CoreMutex lock(&discBoardsMutex);   //we're modifying shared data (across multiple tasks) - lock it
    //remove boards last seen more than 60 minutes ago
    discoveredBoards.erase(
std::remove_if(discoveredBoards.begin(), discoveredBoards.end(),
        [](const DiscoveredBoard& board) {
            const bool bDel = (millis() - board.lastSeen) > MDNS_CACHING_TIMEOUT_MS;
            if (bDel)
                log_info(F("Removing board %s (%s) from list - not seen in last %d minutes"), board.hostname.c_str(), board.ip.toString().c_str(), MDNS_CACHING_TIMEOUT_MS/60000);
            return bDel;
        }),
discoveredBoards.end());

    log_info(F("Total discovered boards: %d"), discoveredBoards.size());
}
#else
const std::vector<DiscoveredBoard> mdns_get_discovered_boards() {
    log_warn(F("mDNS is not enabled, cannot discover boards"));
    return std::vector<DiscoveredBoard>{};
}
#endif
