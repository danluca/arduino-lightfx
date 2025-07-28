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
#include "util.h"
#if LOGGING_ENABLED == 1
#include "stringutils.h"
#include "log.h"
#endif

#define BCAST_QUEUE_TIMEOUT  0     //enqueuing timeout - 0 per https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/05-Software-timers/01-Software-timers

volatile bool fxBroadcastEnabled = false;
BroadcastState broadcastState = Uninitialized;

//broadcast client list, using the last byte of IP addresses - e.g., 192.168.0.10, 192.168.0.11
static constexpr auto syncClientsLSB PROGMEM = {BROADCAST_CLIENTS};     //last byte of the broadcast clients IP addresses (IPv4); assumption that all IP addresses are in the same subnet

static constexpr auto hdContentJson PROGMEM = "Content-Type: application/json";
static constexpr auto hdUserAgent PROGMEM = "User-Agent: rp2040-lightfx-master/1.0.0";
static constexpr auto hdKeepAlive PROGMEM = "Connection: keep-alive";
static constexpr auto fmtFxChange PROGMEM = R"===({"effect":%u,"auto":false,"broadcast":false})===";

QueueHandle_t bcQueue;
static uint16_t tmrTimeUpdateId = 20;
static uint16_t tmrWifiEnsure = 21;
static uint16_t tmrWifiTemp = 22;
static uint16_t tmrStatusLEDCheck = 23;
static uint16_t tmrTimeSetup = 24;

//function declarations ahead
void commInit();
void fxBroadcast(uint16_t index);
void timeUpdate();
void timeSetupCheck();
void enqueueTimeUpdate(TimerHandle_t xTimer);
void enqueueTimeSetup(TimerHandle_t xTimer);
// broadcast task definition - priority is overwritten during setup, see broadcastSetup
FixedQueue<IPAddress*, 10> fxBroadcastRecipients;       //max 10 sync recipients
TimerHandle_t thTimeSetupTimer = nullptr;

/**
 * Structure of the message sent to the broadcast task - internal use only
 */
struct bcTaskMessage {
    enum Action:uint8_t {TIME_SETUP, TIME_UPDATE, FX_SYNC, WIFI_ENSURE, WIFI_TEMP, STATUS_LED_CHECK} event;
    uint16_t data;
};

/**
 * Preparations for broadcast effect changes - set up the recipient clients (others than self), the event posting attributes
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
        log_info(F("FX Broadcast recipient %s has been registered"), clientAddr->toString().c_str());
    }
    broadcastState = Configured;
    log_info(F("FX Broadcast setup completed - %zu clients registered"), fxBroadcastRecipients.size());
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
        case bcTaskMessage::FX_SYNC: fxBroadcast(msg->data); break;
        case bcTaskMessage::WIFI_ENSURE: wifi_ensure(); break;
        case bcTaskMessage::WIFI_TEMP: wifi_temp(); break;
        case bcTaskMessage::STATUS_LED_CHECK: state_led_update(); break;
        default:
            log_error(F("Event type %hd not supported"), msg->event);
            break;
    }

    delete msg;
}

/**
 * Callback for timeUpdate timer - this is called from the Timer task. Enqueues a TIME_UPDATE message for the broadcast task.
 * @param xTimer the timeUpdate timer that fired the callback
 */
void enqueueTimeUpdate(TimerHandle_t xTimer) {
    auto *msg = new bcTaskMessage{bcTaskMessage::TIME_UPDATE, 0};   //gets deleted in execute method upon message receipt
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult == pdFALSE)
        log_error(F("Error sending TIME_UPDATE message to broadcast task for timer %d [%s] - error %ld"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer), qResult);
    // else
    //     log_infoln(F("Sent TIME_UPDATE event successfully to broadcast task for timer %d [%s]"), pvTimerGetTimerID(xTimer), pcTimerGetName(xTimer));
}

/**
 * Enqueues a FX_SYNC event onto the broadcast task - called from FX task.
 */
void enqueueFxUpdate(const uint16_t index) {
    auto *msg = new bcTaskMessage{bcTaskMessage::FX_SYNC, index};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, pdMS_TO_TICKS(BCAST_QUEUE_TIMEOUT)); qResult == pdFALSE)
        log_error(F("Error sending FX_SYNC message to broadcast task for FX %d - error %ld"), index, qResult);
    // else
    //     log_infoln(F("Sent FX_SYNC event successfully to broadcast task for FX %d"), index);
}

/**
 * Callback for timeSetup timer - this is called from the Timer task. Enqueues a TIME_SETUP message for the broadcast task.
 * @param xTimer the timeSetup timer that fired the callback
 */
void enqueueTimeSetup(TimerHandle_t xTimer) {
    auto *msg = new bcTaskMessage{bcTaskMessage::TIME_SETUP, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE)
        log_error(F("Error sending TIME_SETUP message to BC queue for timer %s - error %ld"), xTimer == nullptr ? "on-demand" : pcTimerGetName(xTimer), qResult);
    // else
    //     log_infoln(F("Sent TIME_SETUP event successfully to BC queue for timer %s"), xTimer == nullptr ? "on-demand" : pcTimerGetName(xTimer));
}

void enqueueWifiEnsure(TimerHandle_t xTimer) {
    auto *msg = new bcTaskMessage{bcTaskMessage::WIFI_ENSURE, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE)
        log_error(F("Error sending WIFI_ENSURE message to BC queue for timer %d [%s] - error %ld"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer), qResult);
}

void enqueueWifiTempRead(TimerHandle_t xTimer) {
    auto *msg = new bcTaskMessage{bcTaskMessage::WIFI_TEMP, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE)
        log_error(F("Error sending WIFI_TEMP message to BC queue for timer %d [%s] - error %ld"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer), qResult);
}

/**
 * Callback for statusLEDCheck timer - this is called from the Timer task. Enqueues a STATUS_LED_CHECK message for the alarm task.
 * @param xTimer the statusLEDCheck timer that fired the callback
 */
void enqueueStatusLEDCheck(TimerHandle_t xTimer) {
    auto *msg = new bcTaskMessage{bcTaskMessage::STATUS_LED_CHECK, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE)
        log_error(F("Error sending STATUS_LED_CHECK message to BC queue for timer %d [%s] - error %d"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer), qResult);
    // else
    //     log_info(F("Sent STATUS_LED_CHECK event successfully to BC queue for timer %d [%s]"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer));
}

/**
 * Single client update - identified through IP address
 * @param ip recipient's IP address
 * @param fxIndex effect index to update
 */
void clientUpdate(const IPAddress *ip, const uint16_t fxIndex) {
    log_info(F("Attempting to connect to client %s for FX %hu"), ip->toString().c_str(), fxIndex);
    WiFiClient wiFiClient;  //wifi client - does not need an explicit pointer for underlying WiFi class/driver
    HttpClient client(wiFiClient, *ip, HttpClient::kHttpPort);
    client.setTimeout(1000);
    client.setHttpResponseTimeout(2000);
    client.connectionKeepAlive();
    client.noDefaultRequestHeaders();

    const size_t sz = sprintf(nullptr, fmtFxChange, fxIndex);
    char buf[sz+1];
    sprintf(buf, fmtFxChange, fxIndex);
    buf[sz] = 0;    //null-terminated string

    client.beginRequest();
    //client.put is where the connection is established
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
#if LOGGING_ENABLED == 1
        if (statusCode / 100 == 2)
            log_info(F("Successful sync FX %hu with client %s: %d response status\nBody: %s"), fxIndex, ip->toString().c_str(), statusCode, response.c_str());
        else
            log_error(F("Failed to sync FX %hu to client %s: %d response status"), fxIndex, ip->toString().c_str(), statusCode);
#else
        (void)statusCode;
#endif

    } else
        log_error(F("Failed to connect to client %s, FX %hu not synced"), ip->toString().c_str(), fxIndex);
    client.stop();
}

/**
 * Effect broadcast callback
 * @param index the effect index to broadcast
 */
void fxBroadcast(const uint16_t index) {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        log_warn(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot perform FX  update for %d. System status: %#hX"),
            index, sysInfo->getSysStatus());
        return;
    }

    const LedEffect *fx = fxRegistry.getEffect(index);
    if (!fxBroadcastEnabled) {
        log_warn(F("This board is not a master (FX Broadcast disabled) - will not push effect %s [%hu] to others"), fx->name(), fx->getRegistryIndex());
        return;
    }
    broadcastState = Broadcasting;
    log_info(F("Fx change event - start broadcasting %s [%hu] to %d recipients"), fx->name(), fx->getRegistryIndex(), fxBroadcastRecipients.size());
    for (const auto &client : fxBroadcastRecipients)
        clientUpdate(client, fx->getRegistryIndex());
    log_info(F("Finished broadcasting to %hu recipients - check individual log statements for status of each recipient"), fxBroadcastRecipients.size());
    broadcastState = Waiting;
}

/**
 * Starts or resets the timer responsible for the time setup sequence.
 * If the timer does not already exist, it creates a one-shot timer configured to trigger the
 * `enqueueTimeSetup` callback after a delay of 5 seconds.
 * Logs an error if the timer cannot be created or started.
 */
void startTimeSetupTimer() {
    if (thTimeSetupTimer != nullptr) {
        if (xTimerReset(thTimeSetupTimer, 0) != pdPASS)
            log_error(F("Cannot reset the timeSetup timer - Ignored."));
        return;
    }
    thTimeSetupTimer = xTimerCreate("timeSetup", pdMS_TO_TICKS(60 * 1000), pdFALSE, &tmrTimeSetup, enqueueTimeSetup);
    if (thTimeSetupTimer == nullptr) {
        log_error(F("Cannot create timeSetup timer - Ignored."));
        return;
    }
    if (xTimerStart(thTimeSetupTimer, 0) != pdPASS)
        log_error(F("Cannot start the timeSetup timer - Ignored."));
}

/**
 * Update time with NTP, assert offset (DST or not) and track drift
 */
void timeUpdate() {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        log_error(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot perform NTP time sync. System status: %#hX"), sysInfo->getSysStatus());
        return;
    }
    timeBegin();    //ensures we have network connectivity infrastructure
    const bool bHadNtpSync = sysInfo->isSysStatus(SYS_STATUS_NTP);
    if (const time_t syncElapsedHours = (millis() - timeService.syncLocalTimeMillis())/1000/SECS_PER_HOUR; bHadNtpSync && syncElapsedHours < 12) {
        log_info(F("Time NTP sync was already performed recently %lld hours ago. Skipping - we want to check NTP at least 12 hours apart"), syncElapsedHours);
        return;    //we already did the sync recently, so no need to do it again
    }
    const bool result = timeService.syncTimeNTP();
    if (result) {
        const TimeSync tSync {.localMillis = static_cast<ulong>(timeService.syncLocalTimeMillis()), .unixMillis=timeService.syncUTCTimeMillis() };
        timeSyncs.push(tSync);
        log_info(F("NTP sync success; current time %s"), TimeFormat::asStringMs(nowMillis()).c_str());
        updateLoggingTimebase();
    } else
        log_warn(F("No NTP; Current time %s."), TimeFormat::asStringMs(nowMillis()).c_str());
    result ? sysInfo->setSysStatus(SYS_STATUS_NTP) : sysInfo->resetSysStatus(SYS_STATUS_NTP);
    log_info(F("System status: %#hX"), sysInfo->getSysStatus());

    // if we did not have NTP sync before, react to the current attempt result - if failed, schedule a timer to try again; if succeeded, notify the alarm task for setup
    if (!bHadNtpSync) {
        if (result)
            enqueueAlarmSetup();
        else {
            log_warn(F("Time NTP sync was never acquired. Trying again soon. System status: %#hX"), sysInfo->getSysStatus());
            startTimeSetupTimer();
            return;
        }
    }

    //check for a DST transition
    const time_t nixTime = now();
    if (const bool dst = timeService.timezone()->isDST(nixTime); dst != sysInfo->isSysStatus(SYS_STATUS_DST)) {
        dst ? sysInfo->setSysStatus(SYS_STATUS_DST) : sysInfo->resetSysStatus(SYS_STATUS_DST);
#if LOGGING_ENABLED == 1
        log_info(F("Time DST status changed to %s [offset %d] - current time %s"), dst ? "ON" : "OFF", timeService.timezone()->getOffset(nixTime),
            TimeFormat::asString(nixTime).c_str());
#endif
    }
    if (timeSyncs.size() > 1) {
        //log the current drift
        const auto fromSync = timeSyncs.end()[-2];  //second before last
        const auto toSync = timeSyncs.end()[-1];    //last
        const int driftMs = getDrift(fromSync, toSync);
        timeService.addDrift(-driftMs); //adjust for the drift
        log_info(F("Current drift between %s and %s (%lld ms) measured as %d ms"), TimeFormat::asStringMs(fromSync.unixMillis).c_str(),
            TimeFormat::asStringMs(toSync.unixMillis).c_str(), toSync.unixMillis-fromSync.unixMillis, driftMs);
    }
}

/**
 * Time setup re-attempt, in case we weren't successful during system bootstrap
 */
void timeSetupCheck() {
    if (!sysInfo->isSysStatus(SYS_STATUS_NTP)) {
        if (timeSetup()) {
            //enqueues the alarm setup event if time is ok
            enqueueAlarmSetup();
        } else {
            log_warn(F("System time not yet synchronized with NTP, skipping alarm setup. Reattempting time sync later."));
            startTimeSetupTimer();
        }
    } else
        log_info(F("Time was already properly setup, event fired in excess. System status: %#hX"), sysInfo->getSysStatus());
}

/**
 * Called from the main thread - sets up a task and timers for handling events
 */
void commSetup() {
    if (!sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        log_error(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot setup broadcasting. System status: %#hX"), sysInfo->getSysStatus());
        return;
    }
    // create the broadcast queue, used by enqueue methods to send actions and execute method to receive and execute actions
    bcQueue = xQueueCreate(10, sizeof(bcTaskMessage*));

    //time update event - sync - repeat every 17h
    if (TimerHandle_t thSync = xTimerCreate("timeUpdate", pdMS_TO_TICKS(17 * 3600 * 1000), pdTRUE, &tmrTimeUpdateId, enqueueTimeUpdate); thSync == nullptr)
        log_error(F("Cannot create timeUpdate timer - Ignored."));
    else if (xTimerStart(thSync, 0) != pdPASS)
        log_error(F("Cannot start the timeUpdate timer - Ignored."));
    // create a timer to re-check WiFi and ensure connectivity - every 7 minutes
    const TimerHandle_t thWifiEnsure = xTimerCreate("wifiEnsure", pdMS_TO_TICKS(7*60*1000), pdTRUE, &tmrWifiEnsure, enqueueWifiEnsure);
    if (thWifiEnsure == nullptr)
        log_error(F("Cannot create wifiEnsure timer - Ignored. There is NO wifi re-check scheduled"));
    else if (xTimerStart(thWifiEnsure, 0) != pdPASS)
        log_error(F("Cannot start the wifiEnsure timer - Ignored."));
    //create a timer to take WiFi temperature - every 33 s, 1 s off from the diagnostic thread temp read, to avoid overlap
    const TimerHandle_t thWifiTempRead = xTimerCreate("wifiTempRead", pdMS_TO_TICKS(33*1000), pdTRUE, &tmrWifiTemp, enqueueWifiTempRead);
    if (thWifiTempRead == nullptr)
        log_error(F("Cannot create wifiTempRead timer - Ignored. There is NO wifi temperature read scheduled"));
    else if (xTimerStart(thWifiTempRead, 0) != pdPASS)
        log_error(F("Cannot start the wifiTempRead timer - Ignored."));
    //update the status LED - repeated each 5 seconds
    const TimerHandle_t thStatusLED = xTimerCreate("statusLEDCheck", pdMS_TO_TICKS(5 * 1000), pdTRUE, &tmrStatusLEDCheck, enqueueStatusLEDCheck);
    if (thStatusLED == nullptr)
        log_error(F("Cannot create statusLEDCheck timer - Ignored."));
    else if (xTimerStart(thStatusLED, 0) != pdPASS)
        log_error(F("Cannot start the statusLEDCheck timer - Ignored."));

    commInit();

    postTimeSetupCheck();  //ensure we have the time NTP sync
    log_info(F("Communication system setup OK"));
}

/**
 * Abstraction for posting a time setup event to the broadcast task - usually invoked when the WiFi has been reconnected
 * Note: this method can be called from any other thread
 */
void postTimeSetupCheck() {
    if (!sysInfo->isSysStatus(SYS_STATUS_NTP)) {
        //enqueue a time setup in 5 seconds
        startTimeSetupTimer();
    } else
        log_info(F("Time properly setup - no action taken"));
}

/**
 * Abstraction for posting an effect update event to the queue, without inner knowledge of event objects
 * <b>Note:</b> this method is called from Fx thread and the event queueing will cause the broadcast to be executed in the comms thread.
 * @param index the effect index to post update for
 */
void postFxChangeEvent(const uint16_t index) {
    if (broadcastState >= Configured)
        enqueueFxUpdate(index);
    else
        log_warn(F("Broadcast system is not configured yet - effect %hu cannot be synced. Broadcast enabled=%s"), index, StringUtils::asString(fxBroadcastEnabled));
}
