// Copyright (c) by Dan Luca. All rights reserved.
//

#include <FreeRTOS.h>
#include <memory>
#include "SchedulerExt.h"
#include "comms.h"
#include "efx_setup.h"
#include "net_setup.h"
#include "sysinfo.h"
#include "util.h"
#include "task_msg.h"
#include "mesh_http.h"
#include "mesh_sender.h"
#include "broadcast_client.h"
#if LOGGING_ENABLED == 1
#include "stringutils.h"
#include "log.h"
#endif

std::atomic<bool> fxBroadcastEnabled = false;
volatile BroadcastState broadcastState = Uninitialized;

//broadcast client list, using the last byte of IP addresses - e.g., 192.168.0.10, 192.168.0.11
static constexpr auto staticSyncClientsLSB = {STATIC_BROADCAST_CLIENTS};     //last byte of the broadcast clients IP addresses (IPv4); assumption that all IP addresses are in the same subnet

// Owned and accessed by the web/comms task.
BroadcastRecipients fxBroadcastRecipients;       //max 10 sync recipients, using smart pointers
TimerHandle_t thTimeSetupTimer = nullptr;

QueueHandle_t bcQueue;
static uint16_t tmrTimeUpdateId = 20;
static uint16_t tmrWifiEnsure = 21;
static uint16_t tmrWifiTemp = 22;
static uint16_t tmrStatusLEDCheck = 23;
static uint16_t tmrTimeSetup = 24;
static uint16_t tmrScanClients = 25;

//function declarations ahead
void commInit();
void timeUpdate();
void timeSetupCheck();
void enqueueTimeUpdate(TimerHandle_t xTimer);
void enqueueTimeSetup(TimerHandle_t xTimer);
void scanClients();

/**
 * Preparations for broadcast effect changes - set up the recipient clients (others than self), the event posting attributes
 */
void commInit() {
    const auto selfAddr = sysInfo->refIpAddress();
    for (auto &ipLSB : staticSyncClientsLSB) {
        if (selfAddr[3] == ipLSB)
            continue;
        auto clientAddr = std::make_unique<BroadcastClient>(selfAddr, ipLSB);
        clientAddr->setStatic(true);
        log_info(F("FX Broadcast static recipient %s has been registered"), clientAddr->ip.toString().c_str());
        fxBroadcastRecipients.push(std::move(clientAddr));  //moving clientAddr causes it to become invalid - do NOT use it after this statement
    }
    broadcastState = Configured;
    log_info(F("FX Broadcast setup completed - %zu clients registered"), fxBroadcastRecipients.size());
}

/**
 * The ScheduleExt task scheduler executes this in a continuous loop - this is the main dispatching method of the broadcast task
 * Receives events from the broadcast queue and executes appropriate handlers.
 */
void commRun() {
    serviceMeshClients();
    bcTaskMessage msg{};
    //check for a message to be received, return if we don't have any at this time
    if (pdFALSE == xQueueReceive(bcQueue, &msg, 0))
        return;

    //the reception was successful, process the message
    switch (msg.event) {
        case TIME_SETUP: timeSetupCheck(); break;
        case TIME_UPDATE: timeUpdate(); break;
        case FX_SYNC: break; // Latest FX state is published atomically, even if this queue fills.
        case WIFI_ENSURE: wifi_ensure(); break;
        case WIFI_TEMP: wifi_temp(); break;
        case STATUS_LED_CHECK: state_led_update(); break;
        case ENABLE_BROADCAST: {
            const bool syncMode = static_cast<bool>(msg.data);
            if (syncMode && !meshPrepareMaster()) {
                log_warn(F("Cannot become mesh master while the FX restore queue is full"));
                break;
            }
            fxBroadcastEnabled = syncMode; //we need this enabled before we post the event, if we're doing that
            static constexpr FxActionMessage saveStateMsg{SAVE_STATE, 0};
            if (const BaseType_t qResult = xQueueSend(fxQueue, &saveStateMsg, 0); qResult != pdPASS)
                log_error(F("Failed to enqueue FX SAVE_STATE message"));
            break;
        }
        case SCAN_CLIENTS: scanClients(); break;
        default:
            log_error(F("Comm Event type %hd not supported"), msg.event);
            break;
    }
}

/**
 * Callback for timeUpdate timer - this is called from the Timer task. Enqueues a TIME_UPDATE message for the broadcast task.
 * @param xTimer the timeUpdate timer that fired the callback
 */
void enqueueTimeUpdate(TimerHandle_t xTimer) {
    static constexpr bcTaskMessage msg{TIME_UPDATE, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult == pdFALSE) {
        log_error(F("Error sending TIME_UPDATE message to broadcast task for timer %hu [%s] - error %ld"), getTimerId(xTimer), getTimerName(xTimer), qResult);
    }
    // else
    //     log_info(F("Sent TIME_UPDATE event successfully to broadcast task for timer %hu [%s]"), getTimerId(xTimer), getTimerName(xTimer));
}

/**
 * Callback for timeSetup timer - this is called from the Timer task. Enqueues a TIME_SETUP message for the broadcast task.
 * @param xTimer the timeSetup timer that fired the callback
 */
void enqueueTimeSetup(TimerHandle_t xTimer) {
    static constexpr bcTaskMessage msg{TIME_SETUP, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE) {
        log_error(F("Error sending TIME_SETUP message to BC queue for timer %s - error %ld"), xTimer == nullptr ? "on-demand" : getTimerName(xTimer), qResult);
    }
    // else
    //     log_info(F("Sent TIME_SETUP event successfully to BC queue for timer %s"), xTimer == nullptr ? "on-demand" : getTimerName(xTimer));
}

void enqueueWifiEnsure(TimerHandle_t xTimer) {
    static constexpr bcTaskMessage msg{WIFI_ENSURE, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE) {
        log_error(F("Error sending WIFI_ENSURE message to BC queue for timer %hu [%s] - error %ld"), getTimerId(xTimer), getTimerName(xTimer), qResult);
    }
}

void enqueueWifiTempRead(TimerHandle_t xTimer) {
    static constexpr bcTaskMessage msg{WIFI_TEMP, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE) {
        log_error(F("Error sending WIFI_TEMP message to BC queue for timer %d [%s] - error %ld"), *static_cast<uint16_t *>(pvTimerGetTimerID(xTimer)), pcTimerGetName(xTimer), qResult);
    }
}

/**
 * Callback for statusLEDCheck timer - this is called from the Timer task. Enqueues a STATUS_LED_CHECK message for BC queue.
 * @param xTimer the statusLEDCheck timer that fired the callback
 */
void enqueueStatusLEDCheck(TimerHandle_t xTimer) {
    static constexpr bcTaskMessage msg{STATUS_LED_CHECK, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE) {
        log_error(F("Error sending STATUS_LED_CHECK message to BC queue for timer %hu [%s] - error %ld"), getTimerId(xTimer), getTimerName(xTimer), qResult);
    }
}

/**
 * Callback for scanClients timer - this is called from the Timer task. Enqueues a SCAN_CLIENTS message for the alarm task.
 * @param xTimer the scanClients timer that fired the callback
 */
void enqueueScanClients(TimerHandle_t xTimer) {
    constexpr bcTaskMessage msg{SCAN_CLIENTS, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE) {
        log_error(F("Error sending SCAN_CLIENTS message to BC queue for timer %hu [%s] - error %ld"), getTimerId(xTimer), getTimerName(xTimer), qResult);
    }
}

/**
 * Refresh recipient discovery; mesh acknowledgments determine reachability
 */
void scanClients() {
    // Static peers are retried every announcement, including peers previously offline.
    // Reachability comes from mesh acknowledgments rather than blocking ICMP pings.
}

/**
 * Starts or resets the timer responsible for the time setup sequence.
 * If the timer does not already exist, it creates a one-shot timer configured to trigger the
 * `enqueueTimeSetup` callback after a delay of 60 seconds.
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
    if (!sysInfo->isSysStatus(SysStatus::Wifi)) {
        log_error(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot perform NTP time sync. System status: %#hX"), sysInfo->getSysStatus());
        return;
    }
    timeBegin();    //ensures we have network connectivity infrastructure
    const bool bHadNtpSync = sysInfo->isSysStatus(SysStatus::Ntp);
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
    result ? sysInfo->setSysStatus(SysStatus::Ntp) : sysInfo->resetSysStatus(SysStatus::Ntp);
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
    if (const bool dst = timeService.timezone()->isDST(nixTime); dst != sysInfo->isSysStatus(SysStatus::Dst)) {
        dst ? sysInfo->setSysStatus(SysStatus::Dst) : sysInfo->resetSysStatus(SysStatus::Dst);
#if LOGGING_ENABLED == 1
        log_info(F("Time DST status changed to %s [offset %d] - current time %s"), dst ? "ON" : "OFF", timeService.timezone()->getOffset(nixTime),
            TimeFormat::asString(nixTime).c_str());
#endif
    }
    if (timeSyncs.size() > 1) {
        //log the current drift
        const auto fromSync = timeSyncs.end()[-2];
        const auto toSync = timeSyncs.back();
        if (const int driftMs = getDrift(fromSync, toSync); abs(driftMs) > SECS_PER_HOUR * 1000) {
            log_warn(F("Drift between %s and %s (%lld ms) is too high (%d ms; threshold is 1 hr) - no adjustments made to time base"), TimeFormat::asStringMs(fromSync.unixMillis).c_str(),
                TimeFormat::asStringMs(toSync.unixMillis).c_str(), toSync.unixMillis-fromSync.unixMillis, driftMs);
        } else {
            timeService.addDrift(-driftMs); //adjust for the drift
            log_info(F("Current drift between %s and %s (%lld ms) measured as %d ms - time base adjusted"), TimeFormat::asStringMs(fromSync.unixMillis).c_str(),
                TimeFormat::asStringMs(toSync.unixMillis).c_str(), toSync.unixMillis-fromSync.unixMillis, driftMs);
        }
    }
}

/**
 * Time setup re-attempt, in case we weren't successful during system bootstrap
 */
void timeSetupCheck() {
    if (!sysInfo->isSysStatus(SysStatus::Ntp)) {
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
    if (!sysInfo->isSysStatus(SysStatus::Wifi)) {
        log_error(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot setup broadcasting. System status: %#hX"), sysInfo->getSysStatus());
        return;
    }

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
    //scan for clients - repeated each 15 minutes
    const TimerHandle_t thScanClients = xTimerCreate("scanClients", pdMS_TO_TICKS(15 * 60 * 1000), pdTRUE, &tmrScanClients, enqueueScanClients);
    if (thScanClients == nullptr)
        log_error(F("Cannot create scanClients timer - Ignored. There is NO client scan scheduled"));
    else if (xTimerStart(thScanClients, 0) != pdPASS)
        log_error(F("Cannot start the scanClients timer - Ignored."));

    commInit();

    taskDelay(5000);    //delay before starting processing events

    postTimeSetupCheck();  //ensure we have the time NTP sync
    log_info(F("Communication system setup OK"));
}

/**
 * Abstraction for posting a time setup event to the broadcast task - usually invoked when the WiFi has been reconnected
 * Note: this method can be called from any other thread
 */
void postTimeSetupCheck() {
    if (!sysInfo->isSysStatus(SysStatus::Ntp)) {
        //enqueue a time setup in 60 seconds
        startTimeSetupTimer();
    } else
        log_info(F("Time properly setup - no action taken"));
}

/**
 * Publishes the desired effect atomically; web/comms sends it without blocking the FX task.
 * @param index the effect index to post update for
 */
void postFxChangeEvent(const uint16_t index) {
    meshPublishEffect(index);
}

void forEachActiveClientIP(const std::function<void(const arduino::IPAddress&)>& consumer) {
    for (const auto &client : fxBroadcastRecipients) {
        if (client && client->delivery.confirmed(millis())) {
            consumer(client->ip);
        }
    }
}

void forEachKnownClientIP(const std::function<void(const arduino::IPAddress&)>& consumer) {
    for (const auto &client : fxBroadcastRecipients) {
        if (client) {
            consumer(client->ip);
        }
    }
}
