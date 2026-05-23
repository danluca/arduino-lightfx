// Copyright (c) by Dan Luca. All rights reserved.
//
#include <FreeRTOS.h>
#include <LittleFS.h>
#include <TimeLib.h>
#include <StreamUtils.h>
#include "filesystem.h"
#include "web_server.h"
#include "comms.h"
#include "constants.hpp"
#include "diag.h"
#include "efx_setup.h"
#include "FxSchedule.h"
#include "net_setup.h"
#include "sysinfo.h"
#include "util.h"
#include "task_msg.h"
#include "HealthMonitor.h"
#include "index_html.h"
#include "pixel_css.h"
#include "pixel_js.h"
#include "stats_html.h"
#include "stats_css.h"
#include "stats_js.h"
#if MDNS_ENABLED==1
#include <LEAmDNS.h>
#endif
// not including this file as the regex subsystem has a large codebase and increases the flash use by ~300kB; this would be the only use of regex and there are workarounds
// #include "uri/UriRegex.h"

// #include <detail/base64.hpp>

using namespace web;
// using namespace colTheme;
static constexpr auto hdCacheControl = "Cache-Control";
static constexpr auto hdCacheStatic = "public, max-age=2592000, immutable";
static constexpr auto hdCacheJson = "no-cache, no-store";
static constexpr auto serverAgent = "rp2040-luca/1.0.0";
static constexpr auto hdFmtDate = "%4d-%02d-%02d %02d:%02d:%02d CST";
static constexpr auto hdFmtContentDisposition = "inline; filename=\"%s\"";
static constexpr auto msgRequestNotMapped = "URI not mapped to a handler on this server";
static constexpr auto configJsonFilename = "config.json";
static constexpr auto statusJsonFilename = "status.json";
static constexpr auto tasksJsonFilename = "tasks.json";
static constexpr auto filesJsonFilename = "files.json";
static constexpr auto authToken = "KlFpc1dAdFd0eDRXdkVSZg";
static constexpr uint16_t serverPort = 80;
#if MDNS_ENABLED==1
static auto mdnsStatus = false;
#endif

#define WL_STREAM_BUFFER_SIZE   (1024u)

String masterBoardName;
WebServer web::server;
bool web::server_handlers_configured = false;

/**
 * @brief A map that associates file paths with their corresponding in-memory resources for serving static content.
 *
 * This map is stored in flash memory (PROGMEM) and contains key-value pairs where the key is the file path
 * and the value is a constant pointer to the resource content. These resources are used in the web
 * server to serve static files such as HTML, CSS, and JavaScript to clients.
 *
 * The entries in this map represent:
 * - File paths (e.g., "/pixel.css", "/index.html") as keys.
 * - Pointers to in-memory resources (e.g., `pixel_css`, `index_html`) as values.
 */
static const std::map<std::string, const char *> inFlashResources PROGMEM = {
    {"/pixel.css", pixel_css},
    {"/pixel.js", pixel_js},
    {"/index.html", index_html},
    {"/", index_html},
    {"/stats.css", stats_css},
    {"/stats.js", stats_js},
    {"/stats.html", stats_html}
};

/**
 * Adds the current date as an HTTP header for the current outgoing response
 */
void dateHeader(WebClient &client) {
    const time_t curTime = now();
    tmElements_t tm{};
    timeService.breakTime(curTime, tm);
    char buf[64];   //sufficient size for this header; see hdFmtDate value
    snprintf(buf, sizeof(buf), hdFmtDate, tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    client.addResponseHeader(F("Date"), buf);
}

/**
 * Adds the content-disposition HTTP header - file name - for the current outgoing response
 * @param client web client handling current request
 * @param fname file name
 */
void contentDispositionHeader(WebClient &client, const char *fname) {
    char buf[160];  //deemed sufficient size for this header and expected file names length; see hdFmtContentDisposition value
    snprintf(buf, sizeof(buf), hdFmtContentDisposition, fname);
    client.addResponseHeader(F("Content-Disposition"), buf);
}

/**
 * Serializes a JSON document into string and sends it out to the current client awaiting response from the server
 * @param doc JSON document to marshal
 * @param client web server to use for sending JSON out
 * @return number of bytes written in response
 */
size_t web::marshalJson(const JsonDocument &doc, WebClient &client) {
    const size_t docLength = measureJson(doc);
    client.sendHeaders(200, mime::mimeTable[mime::json].mimeType, docLength);
    WriteBufferingStream wbs(client.rawClient(), WL_STREAM_BUFFER_SIZE);
    const size_t bodySz = serializeJson(doc, wbs);
    client.recordBytesWritten(bodySz);  // body bypasses _currentClientWrite; register it for accurate totals
    return bodySz;
}

/**
 * Web request handler - GET /config.json. Builds response dynamically from in-memory system info and effects registry.
 */
void web::handleGetConfig(WebClient &client) {
    dateHeader(client);
    contentDispositionHeader(client, configJsonFilename);
    client.addResponseHeader(hdCacheControl, hdCacheJson);

    JsonDocument doc;
    SysInfo::sysConfig(doc);
    const auto hldList = doc["holidayList"].to<JsonArray>();
    for (uint8_t hi = None; hi <= NewYear; hi++)
        hldList.add(holidayToString(static_cast<Holiday>(hi)));
    const auto fxArray = doc["fx"].to<JsonArray>();
    fxRegistry.describeConfig(fxArray);

    const size_t sz = marshalJson(doc, client);
    doc.clear();
    (void) sz;
    log_info(F("Handler handleGetConfig invoked for %s, response size %zu bytes"), client.request().uri().c_str(), sz);
}

/**
 * Web request handler - GET /status.json. Method invoked by the web server; response sent to the current client awaiting response from the server
 */
void web::handleGetStatus(WebClient &client) {
    dateHeader(client);
    contentDispositionHeader(client, statusJsonFilename);
    client.addResponseHeader(hdCacheControl, hdCacheJson);

    // response body
    JsonDocument doc;
    // System
    doc["watchdogRebootsCount"] = sysInfo->watchdogRebootsCount();
    doc["cleanBoot"] = sysInfo->isCleanBoot();
    if (sysInfo->hasWatchdogReboots())
        doc["lastWatchdogReboot"] = TimeFormat::asString(sysInfo->lastWatchdogReboot());
    const auto wdReboots = doc["watchdogReboots"].to<JsonArray>();
    for (const auto &r : sysInfo->watchdogRebootsSnapshot()) {
        auto obj = wdReboots.add<JsonObject>();
        obj["time"] = TimeFormat::asString(r.time);
        if (r.resetReason) obj["reason"] = r.resetReason;
        if (r.marker) obj["marker"] = r.marker;
        if (r.fxStage) obj["fxStage"] = r.fxStage;
        if (r.fsBlockedAction) obj["fsBlocked"] = r.fsBlockedAction;
    }

    // WiFi
    const auto wifi = doc["wifi"].to<JsonObject>();
    wifi["IP"] = sysInfo->getIpAddress(); //IP Address
    const int32_t rssi = WiFi.RSSI();
    wifi["bars"] = barSignalLevel(rssi); //Wi-Fi signal level
    wifi["rssi"] = rssi;
    wifi["ssid"] = WiFi.SSID();
    // Fx
    const auto fx = doc["fx"].to<JsonObject>();
    uint16_t curFxIndex = 0;
    const char *curFxName = strNR;
    fx[csAuto] = fxRegistry.isAutoRoll();
    fx[csSleepEnabled] = fxRegistry.isSleepEnabled();
    fx["asleep"] = fxRegistry.isAsleep();
    curFxIndex = fxRegistry.curEffectPos();
    if (const EffectInfo *info = fxRegistry.getEffectInfo(curFxIndex); info != nullptr)
        curFxName = info->desc.id;
    fx["autoTheme"] = paletteFactory.isAuto();
    fx["theme"] = holidayToString(paletteFactory.getHoliday()); //could be forced to a fixed value
    fx["index"] = curFxIndex;
    fx["name"] = curFxName;
    fx[csBroadcast] = fxBroadcastEnabled.load();
    fx[csIgnoreWebFx] = (IGNORE_WEB_EFFECT_CHANGES == 1);   // Reflect compile-time ability to ignore web effect changes
    auto lastFx = fx["pastEffects"].to<JsonArray>();
    fxRegistry.pastEffectsRun(lastFx); //ordered earliest to latest (current effect is the last element)
    fx[csBrightness] = stripBrightness.load();
    fx[csBrightnessLocked] = stripBrightnessLocked.load();
    // Master/Slave status
    const auto master = doc["master"].to<JsonObject>();
    master["active"] = fxBroadcastEnabled.load();
    if (fxBroadcastEnabled) {
        const auto activeClients = master["activeClients"].to<JsonArray>();
        forEachActiveClientIP([&activeClients](const IPAddress &ip) {
            (void)activeClients.add(ip.toString());
        });
    } else if (masterBoardName.length() > 0) {
        master["masterBoard"] = masterBoardName;
    }
    const auto knownClients = master["knownClients"].to<JsonArray>();
    forEachKnownClientIP([&knownClients](const IPAddress &ip) {
        (void)knownClients.add(ip.toString());
    });
    // Time
    const auto time = doc["time"].to<JsonObject>();
    time["ntpSync"] = sysInfo->isSysStatus(SysStatus::Ntp);
    time["millis"] = millis(); //current time in ms
    const time_t curTime = now();
    time["sdate"] = TimeFormat::dateAsString(curTime);  //string date
    time["stime"] = TimeFormat::timeAsString(curTime);  //string time
    time["time"] = curTime; //numeric time
    const bool bDST = sysInfo->isSysStatus(SysStatus::Dst);
    time["dst"] = bDST;
    time["zoneDST"] = timeService.timezone()->isDST(curTime);
    time["offset"] = timeService.timezone()->getOffset(curTime);
    time["zone"] = timeService.timezone()->getName();
    time["zoneShort"] = timeService.timezone()->getShort(curTime);
    time[csHoliday] = holidayToString(currentHoliday()); //time derived holiday
    time["syncSize"] = timeSyncs.size();
    time["averageDrift"] = getAverageTimeDrift();
    time["lastDrift"] = getLastTimeDrift();
    time["totalDrift"] = getTotalDrift();
    time["currentDrift"] = timeService.getDrift();
    const auto syncs = time["syncs"].to<JsonArray>();
    for (const auto &[localMillis, unixMillis]: timeSyncs) {
        auto jts = syncs.add<JsonObject>();
        jts["localMillis"] = localMillis;
        jts["unixMillis"] = unixMillis;
    }
    const auto alarms = time["alarms"].to<JsonArray>();
    for (const auto &al: getScheduledAlarmsCopy()) {
        auto jal = alarms.add<JsonObject>();
        jal["timeLong"] = al.value;
        jal["timeFmt"] = TimeFormat::asString(al.value);
        jal["type"] = alarmTypeToString(al.type);
    }
    //System
    const auto temp = doc["temp"].to<JsonObject>();
    const auto cpuTemp = temp["cpu"].to<JsonObject>();
    cpuTemp["current"] = cpuTempRange.ref.value;
    cpuTemp["current_adc"] = cpuTempRange.ref.adcRaw;
    cpuTemp["max"] = cpuTempRange.max.value;
    cpuTemp["max_adc"] = cpuTempRange.max.adcRaw;
    cpuTemp["min"] = cpuTempRange.min.value;
    cpuTemp["min_adc"] = cpuTempRange.min.adcRaw;
    const auto vcc = doc["vcc"].to<JsonObject>();
    vcc["current"] = lineVoltage.current.value;
    vcc["max"] = lineVoltage.max.value;
    vcc["min"] = lineVoltage.min.value;
    doc["overallStatus"] = static_cast<uint16_t>(sysInfo->getSysStatus());
#if MDNS_ENABLED==1
    doc["mdnsEnabled"] = MDNS.isRunning();
#endif
    //ISO8601 format
    //snprintf(timeBuf, 15, "P%2dDT%2dH%2dM", millis()/86400000l, (millis()/3600000l%24), (millis()/60000%60));
    //human readable format
    char timeBuf[16];
    const unsigned long upTime = millis();
    snprintf(timeBuf, 15, "%2luD %2luH %2lum", upTime / 86400000l, (upTime / 3600000l % 24), (upTime / 60000 % 60));
    doc["upTime"] = timeBuf;
    doc["bootTime"] = upTime;
    const auto cpuTempCal = doc["cpuTempCal"].to<JsonObject>();
    cpuTempCal["valid"] = calibCpuTemp.isValid();
    cpuTempCal["refTemp"] = calibCpuTemp.refTemp;
    cpuTempCal["vtRef"] = calibCpuTemp.vtref;
    cpuTempCal["slope"] = calibCpuTemp.slope;
    cpuTempCal["refDelta"] = calibCpuTemp.refDelta;
    cpuTempCal["refTime"] = calibCpuTemp.time;
    cpuTempCal["minTempVal"] = calibTempMeasurements.min.value;
    cpuTempCal["minTempADC"] = calibTempMeasurements.min.adcRaw;
    cpuTempCal["minTempTime"] = calibTempMeasurements.min.time;
    cpuTempCal["maxTempVal"] = calibTempMeasurements.max.value;
    cpuTempCal["maxTempADC"] = calibTempMeasurements.max.adcRaw;
    cpuTempCal["maxTempTime"] = calibTempMeasurements.max.time;
    cpuTempCal["refTempVal"] = calibTempMeasurements.ref.value;
    cpuTempCal["refTempADC"] = calibTempMeasurements.ref.adcRaw;
    cpuTempCal["refTempTime"] = calibTempMeasurements.ref.time;

    const size_t sz = marshalJson(doc, client);
    doc.clear();
    (void) sz;
    log_info(F("Handler handleGetStatus invoked for %s, response size %zu bytes"), client.request().uri().c_str(), sz);
}

/**
 * Web request handler - PUT /fx. Method invoked by the web server; response sent to the current client awaiting response from the server
 */
void web::handlePutConfig(WebClient &client) {
    dateHeader(client);
    client.addResponseHeader(hdCacheControl, hdCacheJson);

    // Determine origin of the request (UI vs board/other)
    const WebRequest &req = client.request();
    const String userAgent = req.header("User-Agent");
    const String xSource = req.header(kHeaderXSource);
    // const bool isUi = xSource.equalsIgnoreCase(kXSourceUi);
    // const bool isBoard = xSource.equalsIgnoreCase(kXSourceBoard) || userAgent.startsWith(kUaBoardPrefix);
    String body = req.body();

    //process the body - parse JSON body and react to inputs
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, body);
    if (error) {
        client.send(500, mime::mimeTable[mime::txt].mimeType, error.c_str());
        doc.clear();
        return;
    }
    JsonDocument resp;
    const auto upd = resp["updates"].to<JsonObject>();
    BaseType_t qResult = pdTRUE;
    // Effect change requests may be ignored based on compile-time flag and origin
    if (doc[csAuto].is<bool>()) {
#if IGNORE_WEB_EFFECT_CHANGES == 1
        if (!isUi) {
            log_warn(F("Ignoring AUTO_FX change from origin ua='%s', x-source='%s'"), userAgent.c_str(), xSource.c_str());
        } else
#endif
        {
            const bool autoAdvance = doc[csAuto].as<bool>();
            const FxActionMessage msg = {AUTO_FX, autoAdvance};
            if ((qResult = xQueueSend(fxQueue, &msg, 0)) != pdTRUE)
                log_error(F("Error sending AUTO_FX message to FX queue with value %d - error %ld"), autoAdvance, qResult);
            upd[csAuto] = autoAdvance;
        }
    }
    if (doc[strEffect].is<uint16_t>()) {
#if IGNORE_WEB_EFFECT_CHANGES == 1
        if (!isUi) {
            log_warn(F("Ignoring MANUAL_FX change from origin ua='%s', x-source='%s'"), userAgent.c_str(), xSource.c_str());
            resp["talkToHand"] = true;
        } else
#endif
        {
            const auto nextFx = doc[strEffect].as<uint16_t>();
            const FxActionMessage msg = {MANUAL_FX, nextFx};
            if ((qResult = xQueueSend(fxQueue, &msg, 0)) != pdTRUE)
                log_error(F("Error sending MANUAL_FX message to FX queue with value %d - error %ld"), nextFx, qResult);
            upd[strEffect] = nextFx;
            if (doc["source"].is<String>()) {
                const auto source = doc["source"].as<String>();
                masterBoardName = source;
            }
        }
    }
    if (doc[csHoliday].is<String>()) {
        const auto userHoliday = doc[csHoliday].as<String>();
        const uint holiday = parseHoliday(&userHoliday);
        const FxActionMessage msg = {COLOR_THEME, holiday};
        if ((qResult = xQueueSend(fxQueue, &msg, 0)) != pdTRUE)
            log_error(F("Error sending COLOR_THEME message to FX queue with value %u - error %ld"), holiday, qResult);
        upd[csHoliday] = holiday;
    }
    if (doc[csBrightness].is<uint8_t>()) {
        const auto br = doc[csBrightness].as<uint8_t>();
        const FxActionMessage msg = {STRIP_BRIGHTNESS, br};
        if ((qResult = xQueueSend(fxQueue, &msg, 0)) != pdTRUE)
            log_error(F("Error sending COLOR_THEME message to FX queue with value %u - error %ld"), br, qResult);
        upd[csBrightness] = br;
        upd[csBrightnessLocked] = br > 0;
    }
    if (doc[csSleepEnabled].is<bool>()) {
        const bool sleepEnabled = doc[csSleepEnabled].as<bool>();
        const FxActionMessage msg = {SLEEP_ENABLED, sleepEnabled};
        if ((qResult = xQueueSend(fxQueue, &msg, 0)) != pdTRUE)
            log_error(F("Error sending SLEEP_ENABLED message to FX queue with value %d - error %ld"), sleepEnabled, qResult);
        upd[csSleepEnabled] = sleepEnabled;
    }
    if (doc[csResetCal].is<bool>()) {
        if (const bool resetCal = doc[csResetCal].as<bool>()) {
            constexpr DiagAction msg = RESET_CALIBRATION;
            if ((qResult = xQueueSend(diagQueue, &msg, 0)) != pdTRUE)
                log_error(F("Error sending RESET_CALIBRATION message to DIAG queue with value %d - error %ld"), resetCal, qResult);
            upd[csResetCal] = resetCal;
        }
    }
    if (doc[csBroadcast].is<bool>()) {
#if IGNORE_WEB_EFFECT_CHANGES == 1
        if (!isUi) {
            log_warn(F("Ignoring ENABLE_BROADCAST change from origin ua='%s', x-source='%s'"), userAgent.c_str(), xSource.c_str());
        } else
#endif
        {
            const bool syncMode = doc[csBroadcast].as<bool>();
            auto msg = bcTaskMessage{ENABLE_BROADCAST, static_cast<uint16_t>(syncMode)};
            if ((qResult = xQueueSend(bcQueue, &msg, 0)) != pdTRUE) {
                log_error(F("Error sending ENABLE_BROADCAST message to COMM queue with value %d - error %ld"), syncMode, qResult);
            } else
                upd[csBroadcast] = syncMode;
        }
    }
    uint16_t curFxPos = fxRegistry.curEffectPos();
    bool autoRoll = fxRegistry.isAutoRoll();
    bool sleepEnabled = fxRegistry.isSleepEnabled();
    const Holiday holiday = paletteFactory.getHoliday();

    log_info(F("FX: Current config updated effect %hu, autoswitch %s, sleep %s, holiday %s, brightness %hu, brightness adjustment %s"),
        curFxPos, StringUtils::asString(autoRoll), StringUtils::asString(sleepEnabled),
        holidayToString(holiday), stripBrightness.load(), stripBrightnessLocked?"fixed":"automatic");

    //main status and headers
    resp["status"] = qResult == pdTRUE;

    contentDispositionHeader(client, statusJsonFilename);
    //send it out
    const size_t sz = marshalJson(resp, client);
    resp.clear();
    doc.clear();
    (void) sz;
    log_info(F("Handler handlePutConfig invoked for %s, response size %zu bytes"), client.request().uri().c_str(), sz);
}

/**
 * Web request handler - GET /tasks.json. Method invoked by the web server; response sent to the current client awaiting response from the server
 */
void web::handleGetTasks(WebClient &client) {
    dateHeader(client);
    contentDispositionHeader(client, tasksJsonFilename);
    client.addResponseHeader(hdCacheControl, hdCacheJson);

    // response body
    JsonDocument doc;
    doc["cycles32"] = rp2040.getCycleCount();
    doc["cycles64"] = rp2040.getCycleCount64();
    doc["millis"] = millis();
    const time_t curTime = now();
    doc["date"] = TimeFormat::dateAsString(curTime);
    doc["time"] = TimeFormat::timeAsString(curTime);
    auto heap = doc["heap"].to<JsonObject>();
    SysInfo::heapStats(heap);
    auto tasks = doc["tasks"].to<JsonObject>();
    SysInfo::taskStats(tasks);
    doc["boardName"] = sysInfo->getDeviceName();
    doc["boardUid"] = sysInfo->getBoardId();
    doc["fwVersion"] = sysInfo->getBuildVersion();
    doc["fwBranch"] = sysInfo->getScmBranch();
    doc["buildTime"] = sysInfo->getBuildTime();

    //send it out
    const size_t sz = marshalJson(doc, client);
    doc.clear();
    (void) sz;
    log_info(F("Handler handleGetStatus invoked for %s, response size %zu bytes"), client.request().uri().c_str(), sz);
}

/**
 * Special web request handler for resources not found on this server
 */
void web::handleNotFound(WebClient &client) {
    client.send(404, mime::mimeTable[mime::txt].mimeType, msgRequestNotMapped);
    log_info(F("Handler handleNotFound invoked for %s, response size %zu bytes"), client.request().uri().c_str(), strlen(msgRequestNotMapped));
}

struct FWUploadData {
    bool auth{};
    String checkSum;
    String fileName;
};

/**
 * Handles FW image buffered upload leveraging raw handling. This method is called multiple times, where the raw status \code client.raw().status\endcode
 * varies in this order:
 * RAW_START --> RAW_WRITE * n --> RAW_END or RAW_ABORTED
 * @param client web client
 */
void handleFWImageUpload(WebClient &client) {
    const WebRequest &req = client.request();
    switch (HTTPRaw &raw = client.raw(); raw.status) {
        case RAW_START: {
            //check auth token; determine the file name and prepare to stream into it
            const auto fwData = new FWUploadData();
            raw.data = fwData;
            // *auth = req.header("X-Token").equals("*QisW@tWtx4WvERf") ? 0x01 : 0x00;
            fwData->auth = req.header("X-Token").equals(authToken);
            fwData->checkSum = req.header("X-Check");
            fwData->checkSum.toLowerCase();
            fwData->fileName = csFWImageFilename;
            if (fwData->auth) {
                //create a file for the incoming data
                if (SyncFsImpl.exists(fwData->fileName.c_str()))
                    SyncFsImpl.remove(fwData->fileName.c_str());
                markFwUpgradeInitiated();  //mark that FW upgrade stream has started - prevent Core0 from running comms
            }
            log_info(F("FW upload auth %s, size read %zu, size expected %zu, sha-256 expected %s"), fwData->auth ? "OK" : "failed",
                raw.totalSize, req.contentLength(), fwData->checkSum.c_str());
        }
        break;
        case RAW_WRITE:
            //append one raw buffer at a time into the file if auth succeeded
            if (const FWUploadData *fwData = static_cast<FWUploadData *>(raw.data); fwData->auth) {
                SyncFsImpl.appendFile(fwData->fileName.c_str(), raw.buf, raw.currentSize);
            }
            break;
        case RAW_END:
            //this is called at the regular end of raw data processing (i.e., when the amount of data read from the client matches the content-length) - either successful or not
            //close file; send the response to the client as successful receive
            if (const FWUploadData *fwData = static_cast<FWUploadData *>(raw.data); fwData->auth) {
                //determine sha256 hash for the file content - if it matches the source, respond with success, otherwise error out
                if (const String sha256 = SyncFsImpl.sha256(fwData->fileName.c_str()); sha256.equals(fwData->checkSum)) {
                    client.send(200, mime::mimeTable[mime::txt].mimeType, R"({"status": "OK"})");
                    log_info(F("FW upload and storage %s (size read %zu bytes) succeeded - sha-256 actual: %s"), fwData->fileName.c_str(), raw.totalSize, sha256.c_str());
                    if (const TaskHandle_t core0Handle = xTaskGetHandle(csCORE0)) {
                        const BaseType_t fwNotif = xTaskNotify(core0Handle, OTA_UPGRADE_NOTIFY, eSetValueWithOverwrite);
                        log_info(F("CORE0 task has been notified of FW image upload complete, notification status %d"), fwNotif);
                    }
                } else {
                    client.send(406, mime::mimeTable[mime::txt].mimeType, R"({"error": "Upload data integrity failed - Checksum does not match"})");
                    log_error(F("FW upload and storage (size read/expected %zu/%zu bytes) failed - checksum does not match: sha-256 expected: %s, actual: %s"),
                        raw.totalSize, req.contentLength(), fwData->checkSum.c_str(), sha256.c_str());
                    clearFwUpgradeInitiated();
                }
            } else {
                client.send(401, mime::mimeTable[mime::txt].mimeType, R"({"error": "Unauthorized call"})");
                log_error(F("FW upload and storage (size expected %zu bytes) failed - authorization failed"), req.contentLength());
            }
            delete static_cast<FWUploadData *>(raw.data);
            break;
        case RAW_ABORTED: {
            //This is called when the raw data cannot be read from the client any-longer (interrupted). It ends the processing, skips the call for RAW_END
            //close file; delete the file; send the response to the client as an error in receiving
            client.send(400, mime::mimeTable[mime::txt].mimeType, R"({"error": "Bad Request or Read - Aborted"})");
            log_error(F("FW upload and storage (size read/expected %zu/%zu bytes) failed - aborted"), raw.totalSize, req.contentLength());
            const auto fd = static_cast<FWUploadData *>(raw.data);
            if (fd->auth)
                clearFwUpgradeInitiated();
            if (SyncFsImpl.exists(fd->fileName.c_str()))
                SyncFsImpl.remove(fd->fileName.c_str());
            delete fd;
        }
        break;
        default: break;
    }
}

/**
 * Mark that a firmware upgrade has been initiated — enters OTA mode in the health monitor,
 * extending the CORE0 stall threshold to 120 s and resetting the stall timer right before
 * the blocking raw-data read loop begins.
 */
void web::markFwUpgradeInitiated() {
    HealthMonitor::enterOtaMode();
    HealthMonitor::checkIn(HEALTH_CORE0);
    log_info(F("FW upgrade initiated - Core0 will suspend comms during upload"));
}

/**
 * Clear OTA mode and restore normal health monitor thresholds.
 * Call this on any upload failure (aborted or checksum mismatch).
 */
void web::clearFwUpgradeInitiated() {
    HealthMonitor::exitOtaMode();
    log_info(F("FW upgrade flag cleared - resuming normal Core0 operations"));
}

/**
 * Convenience no-op handler. Can also be replaced by a lambda function \code [](WebClient &) { }\endcode
 * @param client web client
 */
void noop(WebClient &client) {}

struct FileUploadData {
    bool auth{};
    String checkSum;
    String fileName;
};

static bool isSafePath(const String &p) {
    if (p.length() == 0) return false;
    if (p[0] != '/') return false;
    if (p.indexOf("..") >= 0) return false;
    if (p.endsWith("/")) return false;
    if (!p.startsWith("/ext/")) return false; // confine all generic uploads under /ext
    if (p.equals(csFWImageFilename)) return false; // do not allow clobbering firmware image via generic upload
    return true;
}

static String normalizePath(String p) {
    // collapse duplicate slashes
    while (p.indexOf("//") >= 0) p.replace("//", "/");
    return p;
}

static bool ensureParentDirs(const String &filePath) {
    const int lastSlash = filePath.lastIndexOf('/');
    if (lastSlash <= 0) return true; // root or no dir
    const String dir = filePath.substring(0, lastSlash);
    // build progressively
    String cur;
    unsigned int start = 0;
    while (start < dir.length()) {
        int slash = dir.indexOf('/', start);
        if (slash < 0) slash = dir.length();
        if (String part = dir.substring(start, slash); part.length() > 0) {
            cur += "/";
            cur += part;
            // try to create; ignore failure (may already exist)
            SyncFsImpl.mkdir(cur.c_str());
        }
        start = slash + 1;
    }
    return true;
}

void handleFileUploadRaw(WebClient &client) {
    const WebRequest &req = client.request();
    switch (HTTPRaw &raw = client.raw(); raw.status) {
        case RAW_START: {
            const auto ud = new FileUploadData();
            raw.data = ud;
            ud->auth = req.header("X-Token").equals(authToken);
            ud->checkSum = req.header("X-Check");
            ud->checkSum.toLowerCase();
            // derive destination path: prefer header X-Path, else first path arg from URI
            String path = req.header("X-Path");
            if (path.length() == 0) {
                // try URI captured group 0
                path = req.pathArg(0);
            }
            if (path.length() > 0) {
                path = Uri::urlDecode(path);
                path = normalizePath(path);
            }
            // Map provided path to /ext root to avoid overwriting system files
            const String userPath = path; // keep for logs
            // Normalize to a relative component under /ext
            if (path.startsWith("/ext/")) {
                path = path.substring(5); // strip "/ext/" prefix if provided
            } else if (path.startsWith("/")) {
                path = path.substring(1); // strip leading slash
            }
            // basic validations on the relative part
            if (path.length() == 0 || path.endsWith("/") || path.indexOf("..") >= 0) {
                ud->auth = false; // force rejection
                client.send(400, mime::mimeTable[mime::txt].mimeType, R"({"error": "Invalid or unsafe path"})");
                log_error(F("Generic upload rejected due to unsafe path '%s'"), userPath.c_str());
                break;
            }
            String fullPath = String("/ext/") + path;
            fullPath = normalizePath(fullPath);
            if (!isSafePath(fullPath)) {
                ud->auth = false; // force rejection
                client.send(400, mime::mimeTable[mime::txt].mimeType, R"({"error": "Invalid or unsafe path"})");
                log_error(F("Generic upload rejected: mapped path outside /ext: '%s' (from '%s')"), fullPath.c_str(), userPath.c_str());
                break;
            }
            ud->fileName = fullPath;
            if (!ensureParentDirs(ud->fileName)) {
                ud->auth = false;
                client.send(500, mime::mimeTable[mime::txt].mimeType, R"({"error": "Failed to create parent directories"})");
                log_error(F("Generic upload failed to ensure parent directories for '%s'"), ud->fileName.c_str());
                break;
            }
            if (ud->auth) {
                if (SyncFsImpl.exists(ud->fileName.c_str()))
                    SyncFsImpl.remove(ud->fileName.c_str());
            }
            log_info(F("File upload start auth %s, dest %s, size expected %zu, sha-256 expected %s"),
                     ud->auth ? "OK" : "failed", ud->fileName.c_str(), req.contentLength(), ud->checkSum.c_str());
        }
        break;
        case RAW_WRITE:
            if (const auto *ud = static_cast<FileUploadData *>(raw.data); ud->auth) {
                SyncFsImpl.appendFile(ud->fileName.c_str(), raw.buf, raw.currentSize);
            }
            break;
        case RAW_END: {
            const auto *ud = static_cast<FileUploadData *>(raw.data);
            if (ud->auth) {
                if (ud->checkSum.length() > 0) {
                    const String sha256 = SyncFsImpl.sha256(ud->fileName.c_str());
                    if (!sha256.equals(ud->checkSum)) {
                        client.send(406, mime::mimeTable[mime::txt].mimeType, R"({"error": "Upload data integrity failed - Checksum does not match"})");
                        log_error(F("Generic upload failed checksum for %s: expected %s, actual %s"), ud->fileName.c_str(), ud->checkSum.c_str(), sha256.c_str());
                        delete ud; break;
                    }
                }
                String body;
                body.reserve(96 + ud->fileName.length());
                body += F("{");
                body += F("\"status\": \"OK\", ");
                body += F("\"path\": \"");
                body += ud->fileName;
                body += F("\", \"size\": ");
                body += String(raw.totalSize);
                body += F("}");
                client.send(200, mime::mimeTable[mime::txt].mimeType, body);
                log_info(F("Generic upload stored %s, size %zu bytes"), ud->fileName.c_str(), raw.totalSize);
            } else {
                client.send(401, mime::mimeTable[mime::txt].mimeType, R"({"error": "Unauthorized call"})");
                log_error(F("Generic upload failed - authorization failed, size expected %zu bytes"), req.contentLength());
            }
            delete ud;
        }
        break;
        case RAW_ABORTED: {
            client.send(400, mime::mimeTable[mime::txt].mimeType, R"({"error": "Bad Request or Read - Aborted"})");
            log_error(F("Generic upload aborted, size read/expected %zu/%zu bytes"), raw.totalSize, req.contentLength());
            const auto ud = static_cast<FileUploadData *>(raw.data);
            if (ud && SyncFsImpl.exists(ud->fileName.c_str()))
                SyncFsImpl.remove(ud->fileName.c_str());
            delete ud;
        }
        break;
        default: break;
    }
}

/**
 * Web request handler - GET /files.json. Returns the list of files/directories on the local filesystem.
 * Path is confined under /ext for safety; optional query parameter `path` can specify a subdirectory.
 */
void handleGetFiles(WebClient &client) {
    dateHeader(client);
    contentDispositionHeader(client, filesJsonFilename);
    client.addResponseHeader(hdCacheControl, hdCacheJson);

    const WebRequest &req = client.request();
    String q = req.arg("path");
    if (q.length() > 0) {
        q = Uri::urlDecode(q);
    }
    // Normalize and map to /ext
    if (q.length() == 0) {
        q = "/"; // default root for listings
    } else {
        q = normalizePath(q);
        if (!q.startsWith("/")) {
            // relative path -> under /
            q = String("/") + q;
        }
    }

    // Safety checks
    if (q.indexOf("..") >= 0 || !q.startsWith("/") ) {
        client.send(400, mime::mimeTable[mime::txt].mimeType, R"({"error": "Invalid or unsafe path"})");
        log_error(F("File list rejected due to unsafe path '%s'"), q.c_str());
        return;
    }

    std::deque<FileInfo> entries{};
    JsonDocument doc;
    const bool ok = SyncFsImpl.list(q.c_str(), &entries);
    doc["basePath"] = q;
    doc["status"] = ok;

    const auto arr = doc["files"].to<JsonArray>();
    if (ok) {
        for (const auto &[name, path, size, modTime, isDir] : entries) {
            auto o = arr.add<JsonObject>();
            // full path and name
            o["path"] = path;
            o["name"] = name;
            o["size"] = static_cast<uint32_t>(size);
            o["isDir"] = isDir;
            // last modified
            o["modifiedEpoch"] = static_cast<long>(modTime);
            if (modTime > 0) {
                o["modified"] = TimeFormat::asString(modTime);
            } else {
                o["modified"] = "";
            }
        }
    }

    entries.clear();

    const size_t sz = marshalJson(doc, client);
    doc.clear();
    (void)sz;
    log_info(F("Handler handleGetFiles invoked for %s, response size %zu bytes"), client.request().uri().c_str(), sz);
}

/**
 * Configures the web server with specific dynamic and static request handlers
 * This method can be called again upon WiFi reconnecting
 */
void web::server_setup() {
    if (!server_handlers_configured) {
        log_info(F("Starting Web server setup"));
        server.setServerAgent(serverAgent);
        // Wire CORE0 health check-in into the raw-transfer loop so the starvation detector
        // stays quiet while a long OTA upload blocks this core
        WebClient::setRawTransferHeartbeat([]() { HealthMonitor::checkIn(HEALTH_CORE0); });
        server.serveStatic("/", SyncFsImpl, "/status/", &inFlashResources, hdCacheStatic);
        server.serveStatic("/file", SyncFsImpl, "/", nullptr, hdCacheStatic);
        server.on("/config.json", HTTP_GET, handleGetConfig);
        server.serveStatic("/health.json", SyncFsImpl, healthEventFileName, nullptr, hdCacheJson);
        server.on("/status.json", HTTP_GET, handleGetStatus);
        server.on("/fx", HTTP_PUT, handlePutConfig);
        server.on("/tasks.json", HTTP_GET, handleGetTasks);
        // Filesystem listing (confined to /ext)
        server.on("/files.json", HTTP_GET, handleGetFiles);
        // Firmware image upload (fixed destination)
        server.on("/fw", HTTP_POST, noop, handleFWImageUpload);
        // Generic file upload: destination via header X-Path or URI context
        server.on("/upload", HTTP_POST, noop, handleFileUploadRaw);
        server.on("/upload", HTTP_PUT, noop, handleFileUploadRaw);
        // server.on(UriRegex("^/upload/(.*)$"), HTTP_POST, noop, handleFileUploadRaw);
        // server.on(UriRegex("^/upload/(.*)$"), HTTP_PUT, noop, handleFileUploadRaw);
        server.onNotFound(handleNotFound);
        server.enableDelay(false); //the task that runs the web-server also runs other services, do not want to introduce unnecessary delays
        server_handlers_configured = true;
        log_info(F("Completed Web server setup"));
    }
    server.collectHeaders("Host", "Accept", "Referer", "User-Agent", "X-Token", "X-Check", "X-Path", kHeaderXSource);
    server.begin(serverPort);
    log_info(F("Web server started"));
}

/**
 * Web Server client handling - one at a time
 */
void web::webserver() {
    server.handleClient();
#if MDNS_ENABLED==1
    if (server.state() == HTTPServer::IDLE)
        mdnsStatus = MDNS.update();
#endif
}
