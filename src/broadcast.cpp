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

FixedQueue<IPAddress*, 10> fxBroadcastRecipients;       //max 10 sync recipients
events::EventQueue broadcastQueue;
events::Event<void(uint16_t)> evBroadcast(&broadcastQueue, fxBroadcast);
events::Event<void(void)> evSetup(&broadcastQueue, broadcastSetup);
rtos::Thread *syncThread;

WiFiClient wiFiClient;  //wifi client - does not need explicit pointer for underlying WiFi class/driver

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

    broadcastState = Configured;
}

/**
 * Single client update - identified through IP address
 * @param ip recipient's IP address
 */
void clientUpdate(const IPAddress *ip, const uint16_t fxIndex) {
    ScopedMutexLock lock(wifiMutex);
    HttpClient client(wiFiClient, *ip, 80);
    client.setTimeout(2000);
    client.setHttpResponseTimeout(2000);
    client.connectionKeepAlive();

    size_t sz = sprintf(nullptr, fmtFxChange, fxIndex);
    char buf[sz+1];
    sprintf(buf, fmtFxChange, fxIndex);
    buf[sz] = 0;    //null terminated string

    client.beginRequest();
    client.put("/fx");
    client.sendHeader(hdContentJson);
    client.sendHeader(hdUserAgent);
    client.sendHeader("Content-Length", sz);
    client.sendHeader(hdKeepAlive);
    client.beginBody();
    client.print(buf);
    client.endRequest();

    int statusCode = client.responseStatusCode();
    String response = client.responseBody();
    if (statusCode/100 == 2)
        Log.infoln(F("Successful FX sync %d with client %p: %d response status\nBody: %s"), fxIndex, ip, statusCode, response.c_str());
    else
        Log.errorln(F("Failed to FX sync %d to client %p: %d response status"), fxIndex,  ip, statusCode);
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
    Log.infoln(F("Completed broadcast to %d recipients"), fxBroadcastRecipients.size());
    broadcastState = Waiting;
}

/**
 * Event timing details setup
 */
void eventSetup() {
    evBroadcast.delay(500ms);
    evSetup.delay(500ms);
}

/**
 * Starts the Sync thread and event posting setup
 */
void startSyncThread() {
    eventSetup();

    //setup the client sync thread - below normal priority
    syncThread = new rtos::Thread(osPriorityBelowNormal, 1280, nullptr, "Sync");
    syncThread->start(callback(&broadcastQueue, &events::EventQueue::dispatch_forever));
    Log.infoln(F("FX Broadcast Sync thread [%s], priority %d - has been setup id %X. Events are dispatching to %d clients"), syncThread->get_name(), syncThread->get_priority(),
               syncThread->get_id(), fxBroadcastRecipients.size());
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
