// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include "broadcast.h"
#include <FreeRTOS.h>
#include <queue.h>
#include <timers.h>
#include "ArduinoHttpClient.h"
#include "WiFiClient.h"
#include "SchedulerExt.h"
#include "efx_setup.h"
#include "net_setup.h"
#include "sysinfo.h"

#define BCAST_QUEUE_TIMEOUT  1000     //enqueuing timeout - 1 second

volatile bool fxBroadcastEnabled = false;
BroadcastState broadcastState = Uninitialized;

using namespace std::chrono;

//broadcast client list, using last byte of IP addresses - e.g. 192.168.0.10, 192.168.0.11
static const uint8_t syncClientsLSB[] PROGMEM = {BROADCAST_CLIENTS};     //last byte of the broadcast clients IP addresses (IPv4); assumption that all IP addresses are in the same subnet

static const char hdContentJson[] PROGMEM = "Content-Type: application/json";
static const char hdUserAgent[] PROGMEM = "User-Agent: rp2040-lightfx-master/1.0.0";
static const char hdKeepAlive[] PROGMEM = "Connection: keep-alive";
static const char fmtFxChange[] PROGMEM = R"===({"effect":%d,"auto":false,"broadcast":false})===";

QueueHandle_t bcQueue;
TaskWrapper* bcTask;
static uint16_t tmrHolidayUpdateId = 20;
static uint16_t tmrTimeUpdateId = 21;
static uint16_t tmrTimeSetupId = 22;

//function declarations ahead
void broadcastInit();
void broadcastExecute();
void fxBroadcast(uint16_t index);
void holidayUpdate();
void timeUpdate();
void timeSetupCheck();
void enqueueHoliday(TimerHandle_t xTimer);
void enqueueTimeUpdate(TimerHandle_t xTimer);
void enqueueTimeSetup(TimerHandle_t xTimer);
// broadcast task definition - priority is overwritten during setup, see broadcastSetup
TaskDef bcDef {broadcastInit, broadcastExecute, 1536, "Sync", 1, CORE_0};
FixedQueue<IPAddress*, 10> fxBroadcastRecipients;       //max 10 sync recipients

/**
 * Structure of the message sent to the broadcast task - internal use only
 */
struct bcTaskMessage {
    enum Action:uint8_t {TIME_SETUP, TIME_UPDATE, HOLIDAY_UPDATE, FX_SYNC} event;
    uint16_t data;
};

/**
 * Preparations for broadcast effect changes - setup the recipient clients (others than self), the event posting attributes
 */
void broadcastInit() {
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
    vTaskDelay(pdMS_TO_TICKS(5000));    //delay before starting processing events
    broadcastState = Configured;
}

/**
 * The ScheduleExt task scheduler executes this in a continuous loop - this is the main dispatching method of the broadcast task
 * Receives events from the broadcast queue and executes appropriate handlers.
 */
void broadcastExecute() {
    bcTaskMessage *msg = nullptr;
    //block indefinitely for a message to be received
    if (pdFALSE == xQueueReceive(bcQueue, &msg, portMAX_DELAY))
        return;
    //the reception was successful, hence the msg is not null anymore
    switch (msg->event) {
        case bcTaskMessage::TIME_SETUP: timeSetupCheck(); break;
        case bcTaskMessage::TIME_UPDATE: timeUpdate(); break;
        case bcTaskMessage::HOLIDAY_UPDATE: holidayUpdate(); break;
        case bcTaskMessage::FX_SYNC:fxBroadcast(msg->data); break;
        default:
            Log.errorln(F("Event type %d not supported"), msg->event);
            break;
    }
    delete msg;
}

/**
 * Callback for holidayUpdate timer - this is called from Timer task or from main task. Enqueues a HOLIDAY_UPDATE message for the broadcast task.
 * @param xTimer the holidayUpdate timer that fired the callback; nullptr when called on-demand (from main)
 */
void enqueueHoliday(TimerHandle_t xTimer) {
    auto *msg = new bcTaskMessage {bcTaskMessage::HOLIDAY_UPDATE, 0};   //gets deleted in execute upon message receipt
    BaseType_t qResult = xQueueSend(bcQueue, &msg, pdMS_TO_TICKS(BCAST_QUEUE_TIMEOUT));
    if (qResult == pdFALSE)
        Log.errorln(F("Error sending HOLIDAY_UPDATE message to broadcast task for timer %d [%s] - error %d"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
    else
        Log.infoln(F("Sent HOLIDAY_UPDATE event successfully to broadcast task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Callback for timeUpdate timer - this is called from Timer task. Enqueues a TIME_UPDATE message for the broadcast task.
 * @param xTimer the timeUpdate timer that fired the callback
 */
void enqueueTimeUpdate(TimerHandle_t xTimer) {
    auto *msg = new bcTaskMessage{bcTaskMessage::TIME_UPDATE, 0};   //gets deleted in execute upon message receipt
    BaseType_t qResult = xQueueSend(bcQueue, &msg, pdMS_TO_TICKS(BCAST_QUEUE_TIMEOUT));
    if (qResult == pdFALSE)
        Log.errorln(F("Error sending TIME_UPDATE message to broadcast task for timer %d [%s] - error %d"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
    else
        Log.infoln(F("Sent TIME_UPDATE event successfully to broadcast task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Enqueues a FX_SYNC event onto the broadcast task - called from FX task.
 */
void enqueueFxUpdate(const uint16_t index) {
    auto *msg = new bcTaskMessage{bcTaskMessage::FX_SYNC, index};
    BaseType_t qResult = xQueueSend(bcQueue, &msg, pdMS_TO_TICKS(BCAST_QUEUE_TIMEOUT));
    if (qResult == pdFALSE)
        Log.errorln(F("Error sending FX_SYNC message to broadcast task for FX %d - error %d"), index, qResult);
    else
        Log.infoln(F("Sent FX_SYNC event successfully to broadcast task for FX %d"), index);
}

/**
 * Callback for timeSetup timer - this is called from Timer task. Enqueues a TIME_SETUP message for the broadcast task.
 * @param xTimer the timeSetup timer that fired the callback
 */
void enqueueTimeSetup(TimerHandle_t xTimer) {
    auto *msg = new bcTaskMessage{bcTaskMessage::TIME_SETUP, 0};
    BaseType_t qResult = xQueueSend(bcQueue, &msg, pdMS_TO_TICKS(BCAST_QUEUE_TIMEOUT));
    if (qResult == pdFALSE)
        Log.errorln(F("Error sending TIME_SETUP message to broadcast task for timer %s - error %d"), xTimer == nullptr ? "on-demand" : pcTimerGetName(xTimer), qResult);
    else
        Log.infoln(F("Sent TIME_SETUP event successfully to broadcast task for timer %s"), xTimer == nullptr ? "on-demand" : pcTimerGetName(xTimer));
}

/**
 * Single client update - identified through IP address
 * @param ip recipient's IP address
 */
void clientUpdate(const IPAddress *ip, const uint16_t fxIndex) {
    CoreMutex lock(&wifiMutex);
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
    //if result is false, do not reset the NTP status, we may have had an older NTP sync
    if (result)
        sysInfo->setSysStatus(SYS_STATUS_NTP);
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
        bool result = ntp_sync();
        result ? sysInfo->setSysStatus(SYS_STATUS_NTP) : sysInfo->resetSysStatus(SYS_STATUS_NTP);
        updateStateLED((result ? CLR_ALL_OK : CLR_SETUP_ERROR).as_uint32_t());
        Log.infoln(F("System status: %X"), sysInfo->getSysStatus());
    } else
        Log.infoln(F("Time was already properly setup, event fired in excess. System status: %X"), sysInfo->getSysStatus());
}

/**
 * Called from main thread - sets up a task and timers for handling events
 */
void broadcastSetup() {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        Log.errorln(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot setup broadcasting. System status: %X"), sysInfo->getSysStatus());
        return;
    }
    //time update event - holiday - repeat every 12h
    TimerHandle_t thHoliday = xTimerCreate("holidayUpdate", pdMS_TO_TICKS(12 * 3600 * 1000), pdTRUE, &tmrHolidayUpdateId, enqueueHoliday);
    if (thHoliday == nullptr)
        Log.errorln(F("Cannot create holidayUpdate timer - Ignored."));
    if (xTimerStart(thHoliday, 0) != pdPASS)
        Log.errorln(F("Cannot start the holidayUpdate timer - Ignored."));

    //time update event - sync - repeat every 17h
    TimerHandle_t thSync = xTimerCreate("timeUpdate", pdMS_TO_TICKS(17 * 3600 * 1000), pdTRUE, &tmrTimeUpdateId, enqueueTimeUpdate);
    if (thSync == nullptr)
        Log.errorln(F("Cannot create timeUpdate timer - Ignored."));
    if (xTimerStart(thSync, 0) != pdPASS)
        Log.errorln(F("Cannot start the timeUpdate timer - Ignored."));

    //setup the client sync thread - below normal priority
    bcQueue = xQueueCreate(10, sizeof(bcTaskMessage*));
    //mirror the priority of the calling task - the broadcast task is intended to have the same priority
    bcDef.priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
    bcTask = Scheduler.startTask(&bcDef);
    Log.infoln(F("FX Broadcast Sync thread [%s], priority %d - has been started with id %X. Events broadcasting enabled=%T"), bcTask->getName(),
               uxTaskPriorityGet(bcTask->getTaskHandle()), uxTaskGetTaskNumber(bcTask->getTaskHandle()), fxBroadcastEnabled);
    postTimeSetupCheck();  //ensure we have the time NTP sync
}

/**
 * Abstraction for posting a time setup event to the broadcast task - usually invoked when the WiFi has been reconnected
 * Note: this method can be called from any other thread
 */
void postTimeSetupCheck() {
    if (!timeClient.isTimeSet())
        enqueueTimeSetup(nullptr);
    else
        Log.infoln(F("Time properly setup - no action taken"));
}

/**
 * Abstraction for posting an effect update event to the queue, without inner knowledge of event objects
 * <b>Note:</b> this method is called from Fx thread and the event queueing will cause the broadcast to be executed in broadcast thread.
 * @param index the effect index to post update for
 */
void postFxChangeEvent(const uint16_t index) {
    if (broadcastState >= Configured)
        enqueueFxUpdate(index);
    else
        Log.warningln(F("Broadcast system is not configured yet - effect %d cannot be synced. Broadcast enabled=%T"), fxBroadcastEnabled);
}

