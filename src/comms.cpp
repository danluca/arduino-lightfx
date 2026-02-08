// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//

#include <FreeRTOS.h>
#include <HTTPClient.h>
#include "SchedulerExt.h"
#include "comms.h"
#include "efx_setup.h"
#include "net_setup.h"
#include "sysinfo.h"
#include "util.h"
#include "task_msg.h"
#if LOGGING_ENABLED == 1
#include "stringutils.h"
#include "log.h"
#endif

#define BCAST_QUEUE_TIMEOUT  0     //enqueuing timeout - 0 per https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/05-Software-timers/01-Software-timers

std::atomic<bool> fxBroadcastEnabled = false;
volatile BroadcastState broadcastState = Uninitialized;

//broadcast client list, using the last byte of IP addresses - e.g., 192.168.0.10, 192.168.0.11
static constexpr auto staticSyncClientsLSB PROGMEM = {STATIC_BROADCAST_CLIENTS};     //last byte of the broadcast clients IP addresses (IPv4); assumption that all IP addresses are in the same subnet

static constexpr auto hdContentJson PROGMEM = "application/json";
static constexpr auto hdUserAgentVersion PROGMEM = "1.0.0";
static constexpr auto fmtFxChange PROGMEM = R"===({"effect":%u,"auto":false,"broadcast":false,"source":"%s"})===";

QueueHandle_t bcQueue;
static uint16_t tmrTimeUpdateId = 20;
static uint16_t tmrWifiEnsure = 21;
static uint16_t tmrStatusLEDCheck = 23;
static uint16_t tmrTimeSetup = 24;
static uint16_t tmrScanClients = 25;

//function declarations ahead
void commInit();
void fxBroadcast(uint16_t index);
void timeUpdate();
void timeSetupCheck();
void enqueueTimeUpdate(TimerHandle_t xTimer);
void enqueueTimeSetup(TimerHandle_t xTimer);
void scanClients();

struct BroadcastClient {
    static constexpr uint8_t BC_ONLINE = 1 << 0;
    static constexpr uint8_t BC_ACTIVE = 1 << 1;
    static constexpr uint8_t BC_STATIC = 1 << 2;

    IPAddress ip;
    unsigned long lastSeenMillis = 0;
    uint8_t flags = BC_ACTIVE;  //bitmap of flags - up to 8 boolean flags

    BroadcastClient(const IPAddress &src, const uint8_t lsb) : ip(src) {
        ip[3] = lsb;
    }

    [[nodiscard]] bool isOnline() const { return flags & BC_ONLINE; }
    void setOnline(const bool online) { if (online) flags |= BC_ONLINE; else flags &= ~BC_ONLINE; }
    [[nodiscard]] bool isActive() const { return flags & BC_ACTIVE; }
    void setActive(const bool active) { if (active) flags |= BC_ACTIVE; else flags &= ~BC_ACTIVE; }
    [[nodiscard]] bool isStatic() const { return flags & BC_STATIC; }
    void setStatic(const bool st) { if (st) flags |= BC_STATIC; else flags &= ~BC_STATIC; }
};

// broadcast task definition - priority is overwritten during setup, see broadcastSetup
FixedQueue<std::unique_ptr<BroadcastClient>, 10> fxBroadcastRecipients;       //max 10 sync recipients, using smart pointers
TimerHandle_t thTimeSetupTimer = nullptr;

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
    bcTaskMessage msg{};
    //check for a message to be received, return if we don't have any at this time
    if (pdFALSE == xQueueReceive(bcQueue, &msg, 0))
        return;

    //the reception was successful, process the message
    switch (msg.event) {
        case TIME_SETUP: timeSetupCheck(); break;
        case TIME_UPDATE: timeUpdate(); break;
        case FX_SYNC: fxBroadcast(msg.data); break;
        case WIFI_ENSURE: wifi_ensure(); break;
        case STATUS_LED_CHECK: state_led_update(); break;
        case ENABLE_BROADCAST: {
            const bool syncMode = static_cast<bool>(msg.data);
            const bool masterEnabled = syncMode != fxBroadcastEnabled && syncMode;
            fxBroadcastEnabled = syncMode; //we need this enabled before we post the event, if we're doing that
            saveFxState();  //persist change immediately
            if (masterEnabled)
                postFxChangeEvent(fxRegistry.curEffectPos()); //we've just enabled broadcasting (this board is a master), issue a sync event to all other boards
            break;
        }
        case SCAN_CLIENTS: scanClients(); break;
        default:
            log_error(F("Event type %hd not supported"), msg.event);
            break;
    }
}

/**
 * Callback for timeUpdate timer - this is called from the Timer task. Enqueues a TIME_UPDATE message for the broadcast task.
 * @param xTimer the timeUpdate timer that fired the callback
 */
void enqueueTimeUpdate(TimerHandle_t xTimer) {
    constexpr bcTaskMessage msg{TIME_UPDATE, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult == pdFALSE) {
        log_error(F("Error sending TIME_UPDATE message to broadcast task for timer %hu [%s] - error %ld"), getTimerId(xTimer), getTimerName(xTimer), qResult);
    }
    // else
    //     log_info(F("Sent TIME_UPDATE event successfully to broadcast task for timer %hu [%s]"), getTimerId(xTimer), getTimerName(xTimer));
}

/**
 * Enqueues a FX_SYNC event onto the broadcast task - called from FX task.
 */
void enqueueFxUpdate(const uint16_t index) {
    const bcTaskMessage msg{FX_SYNC, index};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, pdMS_TO_TICKS(BCAST_QUEUE_TIMEOUT)); qResult == pdFALSE) {
        log_error(F("Error sending FX_SYNC message to broadcast task for FX %d - error %ld"), index, qResult);
    }
    // else
    //     log_info(F("Sent FX_SYNC event successfully to broadcast task for FX %d"), index);
}

/**
 * Callback for timeSetup timer - this is called from the Timer task. Enqueues a TIME_SETUP message for the broadcast task.
 * @param xTimer the timeSetup timer that fired the callback
 */
void enqueueTimeSetup(TimerHandle_t xTimer) {
    constexpr bcTaskMessage msg{TIME_SETUP, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE) {
        log_error(F("Error sending TIME_SETUP message to BC queue for timer %s - error %ld"), xTimer == nullptr ? "on-demand" : getTimerName(xTimer), qResult);
    }
    // else
    //     log_info(F("Sent TIME_SETUP event successfully to BC queue for timer %s"), xTimer == nullptr ? "on-demand" : getTimerName(xTimer));
}

void enqueueWifiEnsure(TimerHandle_t xTimer) {
    constexpr bcTaskMessage msg{WIFI_ENSURE, 0};
    if (const BaseType_t qResult = xQueueSend(bcQueue, &msg, 0); qResult != pdTRUE) {
        log_error(F("Error sending WIFI_ENSURE message to BC queue for timer %hu [%s] - error %ld"), getTimerId(xTimer), getTimerName(xTimer), qResult);
    }
}

/**
 * Callback for statusLEDCheck timer - this is called from the Timer task. Enqueues a STATUS_LED_CHECK message for BC queue.
 * @param xTimer the statusLEDCheck timer that fired the callback
 */
void enqueueStatusLEDCheck(TimerHandle_t xTimer) {
    constexpr bcTaskMessage msg{STATUS_LED_CHECK, 0};
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
 * Ping all clients and record which ones are online
 */
void scanClients() {
#if MDNS_ENABLED == 1
    // Discover boards via mDNS
    const std::vector<DiscoveredBoard>& discoveredBoards = mdns_trim_boards();
    const auto selfAddr = sysInfo->refIpAddress();

    // 1. Update/Add clients from mDNS discovery
    for (const auto &board: discoveredBoards) {
        if (board.ip == selfAddr) continue;

        bool found = false;
        for (const auto &client : fxBroadcastRecipients) {
            if (client && client->ip == board.ip) {
                found = true;
                break;
            }
        }

        if (!found) {
            auto client = std::make_unique<BroadcastClient>(board.ip, board.ip[3]);
            log_info(F("New FX Broadcast recipient %s discovered and registered"), client->ip.toString().c_str());
            fxBroadcastRecipients.push(std::move(client));  //moving client causes it to become invalid - do NOT use it after this statement
        }
    }

    // 2. Remove clients that are no longer discovered and don't respond to ping
    auto it = fxBroadcastRecipients.begin();
    while (it != fxBroadcastRecipients.end()) {
        if (!*it) {
            it = fxBroadcastRecipients.erase(it);
            continue;
        }

        bool discovered = false;
        for (const auto &board : discoveredBoards) {
            if (board.ip == (*it)->ip) {
                discovered = true;
                break;
            }
        }

        const IPAddress ip = (*it)->ip;
        if (!discovered) {
            if (const int resPing = WiFi.ping(ip); resPing >= 0) {
                log_warn(F("FX Broadcast recipient %s is still online but was not discovered by mDNS"), ip.toString().c_str());
                ++it;
            } else if ((*it)->isStatic()) {
                log_info(F("FX Broadcast recipient %s has not been discovered, but it's part of the static list and it won't be removed"), ip.toString().c_str());
                ++it;
            } else {
                log_info(F("FX Broadcast recipient %s is no longer discovered and will be removed"), ip.toString().c_str());
                it = fxBroadcastRecipients.erase(it);
            }
            taskDelay(10);
        } else {
            log_info(F("FX Broadcast recipient %s is still discovered"), ip.toString().c_str());
            ++it;
        }
    }

    log_info(F("FX Broadcast recipients updated - %zu clients registered"), fxBroadcastRecipients.size());
#endif

    // Ping all clients and update status
    for (const auto &client: fxBroadcastRecipients) {
        if (!client) continue;
        //note one ping can take up to 7.5 seconds
        if (const int resPing = WiFi.ping(client->ip); resPing >= 0) {
            client->setOnline(true);
            log_info(F("Client %s is online"), client->ip.toString().c_str());
        } else {
            client->setOnline(false);
            log_warn(F("Client %s is offline"), client->ip.toString().c_str());
        }
        client->lastSeenMillis = millis();
        taskDelay(250);
    }
}

/**
 * Single client update - identified through IP address
 * @param board recipient's details
 * @param fxIndex effect index to update
 */
void clientUpdate(BroadcastClient * const board, const uint16_t fxIndex) {
    if (!board->isOnline() || !board->isActive()) {
        log_warn(F("Client %s is offline [%d] or disabled [%d]. Skipping FX %hu update"), board->ip.toString().c_str(), !board->isOnline(), !board->isActive(), fxIndex);
        return;
    }

    log_info(F("Attempting to connect to online client %s for FX %hu"), board->ip.toString().c_str(), fxIndex);
    WiFiClient wiFiClient;  //Wi-Fi client - does not need an explicit pointer for underlying WiFi class/driver
    HTTPClient client;
    client.setTimeout(750); // keep short to avoid starving the watchdog

    // Use the 4-arg begin overload with host (no scheme), port, and URI
    client.begin(wiFiClient, board->ip.toString(), 80, "/fx");
    client.addHeader("Content-Type", hdContentJson);
    client.addHeader("Connection", "close"); // one-shot request, avoid lingering sockets

    char buf[128];   //size deemed enough based on fmtFxChange pattern and fxIndex values (16bit int)
    const int written = snprintf(buf, sizeof(buf), fmtFxChange, fxIndex, sysInfo->getDeviceName().c_str());
    const int bodyLen = written < 0 ? 0 : written;

    String hdUserAgent;
    hdUserAgent.concat(kUaBoardPrefix);
    hdUserAgent.concat("/");
    hdUserAgent.concat(hdUserAgentVersion);
    client.addHeader("Content-Length", String(bodyLen));
    client.addHeader(kHeaderUserAgent, hdUserAgent);

    if (const int status = client.PUT(buf); status > 0) {
        String response = client.getString();
#if LOGGING_ENABLED == 1
        if (status / 100 == 2)
            log_info(F("Successful sync FX %hu with client %s: %d response status\nBody: %s"), fxIndex, board->ip.toString().c_str(), status, response.c_str());
        else
            log_error(F("Failed to sync FX %hu to client %s: %d response status"), fxIndex, board->ip.toString().c_str(), status);
#endif
        //process the JSON body
        JsonDocument doc;
        if (const DeserializationError error = deserializeJson(doc, response)) {
            log_error(F("Failed to parse JSON response from client %s for FX %hu: %s"), board->ip.toString().c_str(), fxIndex, error.c_str());
        } else {
            const auto brdDisabled = doc["talkToHand"].is<bool>() ? doc["talkToHand"].as<bool>() : false;
            if (brdDisabled) {
                log_info(F("Board %s is online and reports disabled for FX sync: %d response status"), board->ip.toString().c_str(), status);
            }
            board->setActive(!brdDisabled);
            board->setOnline(true);
            board->lastSeenMillis = millis();
        }
        doc.clear();
    } else {
        log_error(F("Failed to connect to client %s (request status %d), FX %hu not synced"), board->ip.toString().c_str(), status, fxIndex);
        board->setOnline(false);
        board->lastSeenMillis = millis();
    }
    client.end();
    taskDelay(100);    //little break in between (multiple) client calls
}

/**
 * Effect broadcast callback
 * @param index the effect index to broadcast
 */
void fxBroadcast(const uint16_t index) {
    if (!sysInfo->isSysStatus(SysStatus::Wifi)) {
        log_warn(F("WiFi was not successfully setup or is currently in process of reconnecting. Cannot perform FX  update for %d. System status: %#hX"),
            index, sysInfo->getSysStatus());
        return;
    }
    const EffectInfo *fxInfo = fxRegistry.getEffectInfo(index);
    if (!fxInfo) {
        log_error(F("Effect at index %d not found"), index);
        return;
    }
    if (!fxBroadcastEnabled) {
        log_warn(F("This board is not a master (FX Broadcast disabled) - will not push effect %s [%hu] to others"), fxInfo->desc.id, index);
        return;
    }
    broadcastState = Broadcasting;
    log_info(F("Fx change event - start broadcasting %s [%hu] to %u recipients"), fxInfo->desc.id, index, static_cast<unsigned>(fxBroadcastRecipients.size()));
    for (const auto &client : fxBroadcastRecipients)
        clientUpdate(client.get(), index);
    log_info(F("Finished broadcasting to %u recipients - check individual log statements for status of each recipient"), static_cast<unsigned>(fxBroadcastRecipients.size()));
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
        const auto fromSync = timeSyncs.end()[-2];  //second before last
        const auto toSync = timeSyncs.end()[-1];    //last
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
    //update the status LED - repeated each 5 seconds
    const TimerHandle_t thStatusLED = xTimerCreate("statusLEDCheck", pdMS_TO_TICKS(5 * 1000), pdTRUE, &tmrStatusLEDCheck, enqueueStatusLEDCheck);
    if (thStatusLED == nullptr)
        log_error(F("Cannot create statusLEDCheck timer - Ignored."));
    else if (xTimerStart(thStatusLED, 0) != pdPASS)
        log_error(F("Cannot start the statusLEDCheck timer - Ignored."));
    //scan for clients - repeated each 5 minutes
    const TimerHandle_t thScanClients = xTimerCreate("scanClients", pdMS_TO_TICKS(5 * 60 * 1000), pdTRUE, &tmrScanClients, enqueueScanClients);
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

void forEachActiveClientIP(const std::function<void(const arduino::IPAddress&)>& consumer) {
    for (const auto &client : fxBroadcastRecipients) {
        if (client && client->isOnline() && client->isActive()) {
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