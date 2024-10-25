// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include "broadcast.h"
#include "ArduinoHttpClient.h"
#include "WiFiClient.h"
#include "rtos.h"
#include "efx_setup.h"
#include "util.h"
#include "sysinfo.h"

#if BROADCAST_MASTER
using namespace std::chrono;

//we'll broadcast to board 2 - 192.168.0.11 static address
static const uint8_t syncClientsLSB[] PROGMEM = {BROADCAST_CLIENTS};     //last byte of the broadcast clients IP addresses (IPv4); assumption that all IP addresses are in the same subnet

static const char hdContentJson[] PROGMEM = "Content-Type: application/json";
static const char hdUserAgent[] PROGMEM = "User-Agent: rp2040-lightfx-master/1.0.0";
static const char hdKeepAlive[] PROGMEM = "Connection: keep-alive";
static const char fmtFxChange[] PROGMEM = "{\"effect\":%d}";

FixedQueue<IPAddress*, 10> fxBroadcastRecipients;       //max 10 sync recipients
events::EventQueue broadcastQueue;
events::Event<void(void)> evBroadcast(&broadcastQueue, fxBroadcast);
rtos::Thread *syncThread;

volatile bool fxEventChange = false;

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
        clientAddr[3] = ipLSB;
        Log.infoln("Client Address: %p", clientAddr);
        fxBroadcastRecipients.push(clientAddr);
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
    client.setTimeout(5000);
    client.setHttpResponseTimeout(10000);

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
        Log.infoln(F("Successful FX sync with client %p: response status %d, body %s"), ip, statusCode, response.c_str());
    else
        Log.errorln(F("Failed to FX sync client %p: response status code %d"), ip, statusCode);
}

void fxBroadcast() {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        Log.warningln(F("No WiFi - Cannot broadcast Fx changes"));
        return;
    }
//    if (!fxEventChange)
//        return;

    for (auto &client : fxBroadcastRecipients)
        clientUpdate(client);
    //broadcast complete - reset the event change flag
    fxEventChange = false;
    Log.infoln(F("Completed broadcast to %d recipients"), fxBroadcastRecipients.size());
}

void postFxChangeEvent() {
    evBroadcast.post();
}

#else

void postFxChangeEvent(){
    //no-op
}

#endif
