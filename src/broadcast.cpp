// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include "broadcast.h"
#include "ArduinoHttpClient.h"
#include "WiFiClient.h"
#include "rtos.h"
#include "efx_setup.h"
#include "net_setup.h"
#include "sysinfo.h"

volatile bool fxBroadcastEnabled = false;
BroadcastState broadcastState = Uninitialized;

using namespace std::chrono;

//broadcast client list, using last byte of IP addresses - e.g. 192.168.0.10, 192.168.0.11
static const uint8_t syncClientsLSB[] PROGMEM = {BROADCAST_CLIENTS};     //last byte of the broadcast clients IP addresses (IPv4); assumption that all IP addresses are in the same subnet

static const char hdContentJson[] PROGMEM = "Content-Type: application/json";
static const char hdUserAgent[] PROGMEM = "User-Agent: rp2040-lightfx-master/1.0.0";
static const char hdKeepAlive[] PROGMEM = "Connection: keep-alive";
static const char fmtFxChange[] PROGMEM = R"===({"effect":%d,"auto":false,"broadcast":false})===";

//function declarations ahead
void broadcastSetup();
void fxBroadcast(uint16_t index);
void holidayUpdate();
void timeUpdate();
void timeSetupCheck();

FixedQueue<IPAddress*, 10> fxBroadcastRecipients;       //max 10 sync recipients
events::EventQueue broadcastQueue;
events::Event<void(void)> evSetup(&broadcastQueue, broadcastSetup);
events::Event<void(uint16_t)> evBroadcast(&broadcastQueue, fxBroadcast);
events::Event<void(void)> evHoliday(&broadcastQueue, holidayUpdate);
events::Event<void(void)> evTimeUpdate(&broadcastQueue, timeUpdate);
events::Event<void(void)> evTimeSetup(&broadcastQueue, timeSetupCheck);
rtos::Thread *syncThread;

/**
 * Preparations for broadcast effect changes - setup the recipient clients (others than self), the event posting attributes
 */
void broadcastSetup() {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        Log.errorln(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot setup broadcasting. System status: %X"), sysInfo->getSysStatus());
        return;
    }

    const String& sysAddr = sysInfo->getIpAddress();
    for (auto &ipLSB : syncClientsLSB) {
        auto clientAddr = new IPAddress();
        clientAddr->fromString(sysAddr);
        if (clientAddr->operator[](3) == ipLSB) {
            delete clientAddr;
            continue;
        }
        clientAddr->operator[](3) = ipLSB;
        fxBroadcastRecipients.push(clientAddr);
        Log.infoln(F("FX Broadcast recipient %p has been registered"), clientAddr);
    }
    Log.infoln(F("FX Broadcast setup completed - %d clients registered"), fxBroadcastRecipients.size());
    broadcastState = Configured;
}

/**
 * Single client update - identified through IP address
 * @param ip recipient's IP address
 */
void clientUpdate(const IPAddress *ip, const uint16_t fxIndex) {
    ScopedMutexLock lock(wifiMutex);
    Log.infoln(F("Attempting to connect to client %p for FX %d"), ip, fxIndex);
    WiFiClient wiFiClient;  //wifi client - does not need explicit pointer for underlying WiFi class/driver
    HttpClient client(wiFiClient, *ip, HttpClient::kHttpPort);
    client.setTimeout(1000);
    client.setHttpResponseTimeout(2000);
    client.connectionKeepAlive();
    client.noDefaultRequestHeaders();

    size_t sz = sprintf(nullptr, fmtFxChange, fxIndex);
    char buf[sz+1];
    sprintf(buf, fmtFxChange, fxIndex);
    buf[sz] = 0;    //null terminated string

    client.beginRequest();
    //client.put is where connection is established
    if (HTTP_SUCCESS == client.put("/fx")) {
        client.sendHeader(hdContentJson);
        client.sendHeader(hdUserAgent);
        client.sendHeader("Content-Length", sz);
        client.sendHeader(hdKeepAlive);
        client.beginBody();
        client.print(buf);
        client.endRequest();

        int statusCode = client.responseStatusCode();
        String response = client.responseBody();
        if (statusCode / 100 == 2)
            Log.infoln(F("Successful sync FX %d with client %p: %d response status\nBody: %s"), fxIndex, ip, statusCode, response.c_str());
        else
            Log.errorln(F("Failed to sync FX %d to client %p: %d response status"), fxIndex, ip, statusCode);
    } else
        Log.errorln(F("Failed to connect to client %p, FX %d not synced"), ip, fxIndex);
    client.stop();
}

/**
 * Effect broadcast callback
 * @param index the effect index to broadcast
 */
void fxBroadcast(const uint16_t index) {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        Log.warningln(F("No WiFi - Cannot broadcast Fx changes"));
        return;
    }

    LedEffect *fx = fxRegistry.getEffect(index);
    if (!fxBroadcastEnabled) {
        Log.warningln(F("This board is not a master (FX Broadcast disabled) - will not push effect %s[%d] to others"), fx->name(), fx->getRegistryIndex());
        return;
    }
    broadcastState = Broadcasting;
    Log.infoln(F("Fx change event - start broadcasting %s[%d] to %d recipients"), fx->name(), fx->getRegistryIndex(), fxBroadcastRecipients.size());
    for (auto &client : fxBroadcastRecipients)
        clientUpdate(client, fx->getRegistryIndex());
    Log.infoln(F("Finished broadcasting to %d recipients - check individual log statements for status of each recipient"), fxBroadcastRecipients.size());
    broadcastState = Waiting;
}

/**
 * Update holiday theme, if needed
 */
void holidayUpdate() {
    Holiday oldHday = paletteFactory.getHoliday();
    Holiday hDay = paletteFactory.adjustHoliday();
#ifndef DISABLE_LOGGING
    if (oldHday == hDay)
            Log.infoln(F("Current holiday remains %s"), holidayToString(hDay));
        else
            Log.infoln(F("Current holiday adjusted from %s to %s"), holidayToString(oldHday), holidayToString(hDay));
#endif
}

/**
 * Update time with NTP, assert offset (DST or not) and track drift
 */
void timeUpdate() {
    bool result = ntp_sync();
    Log.infoln(F("Time NTP sync performed; success = %T"), result);
    if (result && timeSyncs.size() > 2) {
        //log the current drift
        time_t from = timeSyncs.end()[-2].unixSeconds;
        time_t to = now();
        Log.infoln(F("Current drift between %y and %y (%u s) measured as %d ms"), from, to, (long)(to-from), getLastTimeDrift());
    }
}

/**
 * Time setup re-attempt, in case we weren't successful during system bootstrap
 */
void timeSetupCheck() {
    if (!timeClient.isTimeSet()) {
        updateStateLED((time_setup() ? CLR_ALL_OK : CLR_SETUP_ERROR).as_uint32_t());
        Log.infoln(F("System status: %X"), sysInfo->getSysStatus());
    } else
        Log.infoln(F("Time was already properly setup, event fired in excess. System status: %X"), sysInfo->getSysStatus());
}

/**
 * Event timing details setup
 */
void eventSetup() {
    //initial delays for setup and broadcast, these are one time trigger and on demand respectively
    evSetup.delay(500ms);
    evBroadcast.delay(500ms);

    //time update events - holiday and time sync
    evHoliday.delay(1min);
    evHoliday.period(12h);
    evHoliday.post();

    evTimeUpdate.delay(3min);
    evTimeUpdate.period(17h);
    evTimeUpdate.post();

    evTimeSetup.delay(1s);
}

/**
 * Starts the Sync thread and event posting setup
 */
void startSyncThread() {
    eventSetup();

    //setup the client sync thread - below normal priority
    syncThread = new rtos::Thread(osPriorityNormal, 1536, nullptr, "Sync");
    syncThread->start(callback(&broadcastQueue, &events::EventQueue::dispatch_forever));
    Log.infoln(F("FX Broadcast Sync thread [%s], priority %d - has been started with id %X. Events broadcasting enabled=%T"), syncThread->get_name(), syncThread->get_priority(),
               syncThread->get_id(), fxBroadcastEnabled);
}

/**
 * Abstraction for posting an effect update event to the queue, without inner knowledge of event objects
 * <b>Note:</b> this method is called from Fx thread and the mbed Event posting will cause the
 * broadcast callback to be executed in Sync thread.
 * @param index the effect index to post update for
 */
void postFxChangeEvent(const uint16_t index) {
    if (broadcastState >= Configured)
        evBroadcast.post(index);
    else
        Log.warningln(F("Broadcast system is not configured yet - effect %d cannot be synced. Broadcast enabled=%T"), fxBroadcastEnabled);
}

void postWiFiSetupEvent() {
    if (syncThread == nullptr)
        startSyncThread();
    if (broadcastState < Configured)
        evSetup.post();
    else
        Log.infoln(F("WiFi setup event received - the broadcast system is already configured. Broadcast enabled=%T"), fxBroadcastEnabled);
}

void postTimeSetupCheck() {
    if (!timeClient.isTimeSet())
        evTimeSetup.post();
    else
        Log.infoln(F("Time properly setup - no action taken"));
}
