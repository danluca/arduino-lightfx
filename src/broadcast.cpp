// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include "broadcast.h"
#include "ArduinoHttpClient.h"
#include "WiFiClient.h"
#include "rtos.h"
#include "efx_setup.h"
#include "util.h"
#include "sysinfo.h"

volatile bool fxBroadcastEnabled = false;

using namespace std::chrono;

//we'll broadcast to board 2 - 192.168.0.11 static address
static const uint8_t syncClientsLSB[] PROGMEM = {BROADCAST_CLIENTS};     //last byte of the broadcast clients IP addresses (IPv4); assumption that all IP addresses are in the same subnet

static const char hdContentJson[] PROGMEM = "Content-Type: application/json";
static const char hdUserAgent[] PROGMEM = "User-Agent: rp2040-lightfx-master/1.0.0";
static const char hdKeepAlive[] PROGMEM = "Connection: keep-alive";
static const char fmtFxChange[] PROGMEM = R"===({"effect":%d,"auto":false,"broadcast":false})===";

FixedQueue<IPAddress*, 10> fxBroadcastRecipients;       //max 10 sync recipients
events::EventQueue broadcastQueue;
events::Event<void(uint16_t)> evBroadcast(&broadcastQueue, fxBroadcast);
rtos::Thread *syncThread;

WiFiClient wiFiClient;

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

    evBroadcast.delay(500ms);

    //setup the client sync thread
    syncThread = new rtos::Thread(osPriorityNormal, 1280, nullptr, "Sync");
    syncThread->start(callback(&broadcastQueue, &events::EventQueue::dispatch_forever));
    Log.infoln(F("FX Broadcast Sync thread [%s], priority %d - has been setup id %X. Events are dispatching to %d clients"), syncThread->get_name(), syncThread->get_priority(),
               syncThread->get_id(), fxBroadcastRecipients.size());
}

void clientUpdate(const IPAddress *ip) {
    HttpClient client(wiFiClient, *ip, 80);
    client.setTimeout(2000);
    client.setHttpResponseTimeout(5000);
    client.connectionKeepAlive();

    size_t sz = sprintf(nullptr, fmtFxChange, fxRegistry.curEffectPos());
    char buf[sz+1];
    sprintf(buf, fmtFxChange, fxRegistry.curEffectPos());
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
        Log.infoln(F("Successful FX sync with client %p: %d response status\nBody: %s"), ip, statusCode, response.c_str());
    else
        Log.errorln(F("Failed to FX sync client %p: %d response status"), ip, statusCode);
    client.stop();
}

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

    Log.infoln(F("Fx change event - start broadcasting %s[%d] to %d recipients"), fx->name(), fx->getRegistryIndex(), fxBroadcastRecipients.size());
    for (auto &client : fxBroadcastRecipients)
        clientUpdate(client);
    Log.infoln(F("Completed broadcast to %d recipients"), fxBroadcastRecipients.size());
}

void postFxChangeEvent(const uint16_t index) {
    evBroadcast.post(index);
}

