// Copyright (c) 2024,2025 by Dan Luca. All rights reserved.
//

#include <FreeRTOS.h>
#include <queue.h>
#include <timers.h>
#include <ArduinoHttpClient.h>
#include "SchedulerExt.h"
#include "comms.h"
#include "efx_setup.h"
#include "net_setup.h"
#include "sysinfo.h"
#include "ledstate.h"
#include "stringutils.h"
#include "util.h"

#define BCAST_QUEUE_TIMEOUT  0     //enqueuing timeout - 0 per https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/05-Software-timers/01-Software-timers

volatile bool fxBroadcastEnabled = false;
BroadcastState broadcastState = Uninitialized;

//broadcast client list, using last byte of IP addresses - e.g. 192.168.0.10, 192.168.0.11
static constexpr auto syncClientsLSB PROGMEM = {BROADCAST_CLIENTS};     //last byte of the broadcast clients IP addresses (IPv4); assumption that all IP addresses are in the same subnet

static constexpr auto hdContentJson PROGMEM = "Content-Type: application/json";
static constexpr auto hdUserAgent PROGMEM = "User-Agent: rp2040-lightfx-master/1.0.0";
static constexpr auto hdKeepAlive PROGMEM = "Connection: keep-alive";
static constexpr auto fmtFxChange PROGMEM = R"===({"effect":%u,"auto":false,"broadcast":false})===";

QueueHandle_t bcQueue;
static uint16_t tmrHolidayUpdateId = 20;
static uint16_t tmrTimeUpdateId = 21;

//function declarations ahead
void commInit();
void fxBroadcast(uint16_t index);
void holidayUpdate();
void timeUpdate();
void timeSetupCheck();
void enqueueHoliday(TimerHandle_t xTimer);
void enqueueTimeUpdate(TimerHandle_t xTimer);
void enqueueTimeSetup(TimerHandle_t xTimer);
// broadcast task definition - priority is overwritten during setup, see broadcastSetup
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
void commInit() {
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
        Log.info(F("FX Broadcast recipient %s has been registered"), clientAddr->toString().c_str());
    }
    broadcastState = Configured;
    Log.info(F("FX Broadcast setup completed - %zu clients registered"), fxBroadcastRecipients.size());
    taskDelay(5000);    //delay before starting processing events
}

/**
 * The ScheduleExt task scheduler executes this in a continuous loop - this is the main dispatching method of the broadcast task
 * Receives events from the broadcast queue and executes appropriate handlers.
 */
void commRun() {
    bcTaskMessage *msg = nullptr;
    //check for a message to be received, return if we don't have any at this time
    if (pdFALSE == xQueueReceive(bcQueue, &msg, 0))
        return;
    //the reception was successful, hence the msg is not null anymore
    switch (msg->event) {
        case bcTaskMessage::TIME_SETUP: timeSetupCheck(); break;
        case bcTaskMessage::TIME_UPDATE: timeUpdate(); break;
        case bcTaskMessage::HOLIDAY_UPDATE: holidayUpdate(); break;
        case bcTaskMessage::FX_SYNC: fxBroadcast(msg->data); break;
        default:
            Log.error(F("Event type %hd not supported"), msg->event);
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
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult == pdFALSE)
        Log.error(F("Error sending HOLIDAY_UPDATE message to broadcast task for timer %d [%s] - error %ld"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
    // else
    //     Log.infoln(F("Sent HOLIDAY_UPDATE event successfully to broadcast task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Callback for timeUpdate timer - this is called from Timer task. Enqueues a TIME_UPDATE message for the broadcast task.
 * @param xTimer the timeUpdate timer that fired the callback
 */
void enqueueTimeUpdate(TimerHandle_t xTimer) {
    auto *msg = new bcTaskMessage{bcTaskMessage::TIME_UPDATE, 0};   //gets deleted in execute upon message receipt
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult == pdFALSE)
        Log.error(F("Error sending TIME_UPDATE message to broadcast task for timer %d [%s] - error %ld"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
    // else
    //     Log.infoln(F("Sent TIME_UPDATE event successfully to broadcast task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Enqueues a FX_SYNC event onto the broadcast task - called from FX task.
 */
void enqueueFxUpdate(const uint16_t index) {
    auto *msg = new bcTaskMessage{bcTaskMessage::FX_SYNC, index};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, pdMS_TO_TICKS(BCAST_QUEUE_TIMEOUT)); qResult == pdFALSE)
        Log.error(F("Error sending FX_SYNC message to broadcast task for FX %d - error %ld"), index, qResult);
    // else
    //     Log.infoln(F("Sent FX_SYNC event successfully to broadcast task for FX %d"), index);
}

/**
 * Callback for timeSetup timer - this is called from Timer task. Enqueues a TIME_SETUP message for the broadcast task.
 * @param xTimer the timeSetup timer that fired the callback
 */
void enqueueTimeSetup(TimerHandle_t xTimer) {
    auto *msg = new bcTaskMessage{bcTaskMessage::TIME_SETUP, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE)
        Log.error(F("Error sending TIME_SETUP message to broadcast task for timer %s - error %ld"), xTimer == nullptr ? "on-demand" : pcTimerGetName(xTimer), qResult);
    // else
    //     Log.infoln(F("Sent TIME_SETUP event successfully to broadcast task for timer %s"), xTimer == nullptr ? "on-demand" : pcTimerGetName(xTimer));
}

/**
 * Single client update - identified through IP address
 * @param ip recipient's IP address
 * @param fxIndex effect index to update
 */
void clientUpdate(const IPAddress *ip, const uint16_t fxIndex) {
    Log.info(F("Attempting to connect to client %s for FX %hu"), ip->toString().c_str(), fxIndex);
    WiFiClient wiFiClient;  //wifi client - does not need explicit pointer for underlying WiFi class/driver
    HttpClient client(wiFiClient, *ip, HttpClient::kHttpPort);
    client.setTimeout(1000);
    client.setHttpResponseTimeout(2000);
    client.connectionKeepAlive();
    client.noDefaultRequestHeaders();

    const size_t sz = sprintf(nullptr, fmtFxChange, fxIndex);
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

        const int statusCode = client.responseStatusCode();
        String response = client.responseBody();
        if (statusCode / 100 == 2)
            Log.info(F("Successful sync FX %hu with client %s: %d response status\nBody: %s"), fxIndex, ip->toString().c_str(), statusCode, response.c_str());
        else
            Log.error(F("Failed to sync FX %hu to client %s: %d response status"), fxIndex, ip->toString().c_str(), statusCode);
    } else
        Log.error(F("Failed to connect to client %s, FX %hu not synced"), ip->toString().c_str(), fxIndex);
    client.stop();
}

/**
 * Effect broadcast callback
 * @param index the effect index to broadcast
 */
void fxBroadcast(const uint16_t index) {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        Log.warn(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot perform FX  update for %d. System status: %#hX"),
            index, sysInfo->getSysStatus());
        return;
    }

    const LedEffect *fx = fxRegistry.getEffect(index);
    if (!fxBroadcastEnabled) {
        Log.warn(F("This board is not a master (FX Broadcast disabled) - will not push effect %s [%hu] to others"), fx->name(), fx->getRegistryIndex());
        return;
    }
    broadcastState = Broadcasting;
    Log.info(F("Fx change event - start broadcasting %s [%hu] to %d recipients"), fx->name(), fx->getRegistryIndex(), fxBroadcastRecipients.size());
    for (const auto &client : fxBroadcastRecipients)
        clientUpdate(client, fx->getRegistryIndex());
    Log.info(F("Finished broadcasting to %hu recipients - check individual log statements for status of each recipient"), fxBroadcastRecipients.size());
    broadcastState = Waiting;
}

/**
 * Update holiday theme, if needed
 */
void holidayUpdate() {
    const Holiday oldHday = paletteFactory.getHoliday();
    if (Holiday hDay = paletteFactory.adjustHoliday(); oldHday == hDay)
        Log.info(F("Current holiday remains %s"), holidayToString(hDay));
    else
        Log.info(F("Current holiday adjusted from %s to %s"), holidayToString(oldHday), holidayToString(hDay));
}

/**
 * Update time with NTP, assert offset (DST or not) and track drift
 */
void timeUpdate() {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        Log.error(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot perform time update. System status: %#hX"), sysInfo->getSysStatus());
        return;
    }
    const bool result = ntp_sync();
    Log.info(F("Time NTP sync performed; success = %s"), StringUtils::asString(result));
    //if result is false, do not reset the NTP status, we may have had an older NTP sync
    if (result)
        sysInfo->setSysStatus(SYS_STATUS_NTP);
    if (result && timeSyncs.size() > 2) {
        //log the current drift
        const time_t from = timeSyncs.end()[-2].unixSeconds;
        const time_t to = now();
        Log.info(F("Current drift between %s and %s (%u s) measured as %d ms"), StringUtils::asString(from).c_str(), StringUtils::asString(to).c_str(), (long)(to-from), getLastTimeDrift());
    }
}

/**
 * Time setup re-attempt, in case we weren't successful during system bootstrap
 */
void timeSetupCheck() {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        Log.error(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot perform time setup check. System status: %#hX"), sysInfo->getSysStatus());
        return;
    }
    if (!timeClient.isTimeSet()) {
        bool result = ntp_sync();
        result ? sysInfo->setSysStatus(SYS_STATUS_NTP) : sysInfo->resetSysStatus(SYS_STATUS_NTP);
        updateStateLED((result ? CLR_ALL_OK : CLR_SETUP_ERROR).as_uint32_t());
        Log.info(F("System status: %#hX"), sysInfo->getSysStatus());
    } else
        Log.info(F("Time was already properly setup, event fired in excess. System status: %#hX"), sysInfo->getSysStatus());
}

/**
 * Called from main thread - sets up a task and timers for handling events
 */
void commSetup() {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        Log.error(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot setup broadcasting. System status: %#hX"), sysInfo->getSysStatus());
        return;
    }
    // create the broadcast queue, used by enqueue methods to send actions and execute method to receive and execute actions
    bcQueue = xQueueCreate(10, sizeof(bcTaskMessage*));

    //time update event - holiday - repeat every 12h
    if (TimerHandle_t thHoliday = xTimerCreate("holidayUpdate", pdMS_TO_TICKS(12 * 3600 * 1000), pdTRUE, &tmrHolidayUpdateId, enqueueHoliday); thHoliday == nullptr)
        Log.error(F("Cannot create holidayUpdate timer - Ignored."));
    else if (xTimerStart(thHoliday, 0) != pdPASS)
        Log.error(F("Cannot start the holidayUpdate timer - Ignored."));

    //time update event - sync - repeat every 17h
    if (TimerHandle_t thSync = xTimerCreate("timeUpdate", pdMS_TO_TICKS(17 * 3600 * 1000), pdTRUE, &tmrTimeUpdateId, enqueueTimeUpdate); thSync == nullptr)
        Log.error(F("Cannot create timeUpdate timer - Ignored."));
    else if (xTimerStart(thSync, 0) != pdPASS)
        Log.error(F("Cannot start the timeUpdate timer - Ignored."));

    commInit();

    //setup the communications thread - normal priority
    // bcTask = Scheduler.startTask(&bcDef);
    // Log.infoln(F("Communications thread [%s], priority %d - has been started with id %u. Events broadcasting enabled=%T"), bcTask->getName(),
    //            uxTaskPriorityGet(bcTask->getTaskHandle()), uxTaskGetTaskNumber(bcTask->getTaskHandle()), fxBroadcastEnabled);
    postTimeSetupCheck();  //ensure we have the time NTP sync
    Log.info(F("Communication system setup OK"));
}

/**
 * Abstraction for posting a time setup event to the broadcast task - usually invoked when the WiFi has been reconnected
 * Note: this method can be called from any other thread
 */
void postTimeSetupCheck() {
    if (!timeClient.isTimeSet())
        enqueueTimeSetup(nullptr);
    else
        Log.info(F("Time properly setup - no action taken"));
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
        Log.warn(F("Broadcast system is not configured yet - effect %hu cannot be synced. Broadcast enabled=%s"), index, StringUtils::asString(fxBroadcastEnabled));
}

