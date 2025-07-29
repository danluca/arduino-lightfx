// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#include <LittleFS.h>
#include <PicoLog.h>
#include <TimeLib.h>
#include "filesystem.h"
#include "web_server.h"
#include "comms.h"
#include "constants.hpp"
#include "diag.h"
#include "efx_setup.h"
#include "FxSchedule.h"
#include "mic.h"
#include "net_setup.h"
#include "stringutils.h"
#include "sysinfo.h"
#include "util.h"
#include "index_html.h"
#include "pixel_css.h"
#include "pixel_js.h"
#include "stats_html.h"
#include "stats_css.h"
#include "stats_js.h"
// #include <detail/base64.hpp>

using namespace web;
// using namespace colTheme;
static constexpr auto hdCacheControl PROGMEM = "Cache-Control";
static constexpr auto hdCacheStatic PROGMEM = "public, max-age=2592000, immutable";
static constexpr auto hdCacheJson PROGMEM = "no-cache, no-store";
static constexpr auto serverAgent PROGMEM = "rp2040-luca/1.0.0";
static constexpr auto hdFmtDate PROGMEM = "%4d-%02d-%02d %02d:%02d:%02d CST";
static constexpr auto hdFmtContentDisposition PROGMEM = "inline; filename=\"%s\"";
static constexpr auto msgRequestNotMapped PROGMEM = "URI not mapped to a handler on this server";
// static constexpr auto configJsonFilename PROGMEM = "config.json";
static constexpr auto statusJsonFilename PROGMEM = "status.json";
static constexpr auto tasksJsonFilename PROGMEM = "tasks.json";
static constexpr uint16_t serverPort PROGMEM = 80;
#if MDNS_ENABLED==1
static auto mdnsStatus = MDNS::Status::TryLater;
#endif

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
    const int szBuf = snprintf(nullptr, 0, hdFmtDate, year(curTime), month(curTime), day(curTime), hour(curTime), minute(curTime), second(curTime)) + 1;
    char buf[szBuf];
    snprintf(buf, szBuf, hdFmtDate, year(curTime), month(curTime), day(curTime), hour(curTime), minute(curTime), second(curTime));
    client.sendHeader(F("Date"), buf);
}

/**
 * Adds the content-disposition HTTP header - file name - for the current outgoing response
 * @param client web client handling current request
 * @param fname file name
 */
void contentDispositionHeader(WebClient &client, const char *fname) {
    const int szBuf = snprintf(nullptr, 0, hdFmtContentDisposition, fname) + 1;
    char buf[szBuf];
    snprintf(buf, szBuf, hdFmtContentDisposition, fname);
    client.sendHeader(F("Content-Disposition"), buf);
}

/**
 * Serializes a JSON document into string and sends it out to the current client awaiting response from the server
 * @param doc JSON document to marshal
 * @param client web server to use for sending JSON out
 * @return number of bytes written in response
 */
size_t web::marshalJson(const JsonDocument &doc, WebClient &client) {
    //send it out
    const auto buf = new String();
    buf->reserve(5120); // deemed enough for all/most JSON docs in this app (largest is the config file at 4800 bytes)
    serializeJson(doc, *buf);
    const size_t sz = client.send(200, mime::mimeTable[mime::json].mimeType, *buf);
    delete buf;
    return sz;
}

/**
 * Web request handler - GET /status.json. Method invoked by the web server; response sent to the current client awaiting response from the server
 */
void web::handleGetStatus(WebClient &client) {
    dateHeader(client);
    contentDispositionHeader(client, statusJsonFilename);
    client.sendHeader(hdCacheControl, hdCacheJson);

    // response body
    JsonDocument doc;
    // System
    doc["watchdogRebootsCount"] = sysInfo->watchdogReboots().size();
    doc["cleanBoot"] = sysInfo->isCleanBoot();
    if (!sysInfo->watchdogReboots().empty())
        doc["lastWatchdogReboot"] = TimeFormat::asString(sysInfo->watchdogReboots().back());
    const auto wdReboots = doc["watchdogReboots"].to<JsonArray>();
    for (const auto &wd: sysInfo->watchdogReboots())
        wdReboots.add<String>(TimeFormat::asString(wd));

    // WiFi
    const auto wifi = doc["wifi"].to<JsonObject>();
    wifi["IP"] = sysInfo->getIpAddress(); //IP Address
    const int32_t rssi = WiFi.RSSI();
    wifi["bars"] = barSignalLevel(rssi); //Wi-Fi signal level
    wifi["rssi"] = rssi;
    // Fx
    const auto fx = doc["fx"].to<JsonObject>();
    fx[csAuto] = fxRegistry.isAutoRoll();
    fx[csSleepEnabled] = fxRegistry.isSleepEnabled();
    fx["asleep"] = fxRegistry.isAsleep();
    fx["autoTheme"] = paletteFactory.isAuto();
    fx["theme"] = holidayToString(paletteFactory.getHoliday()); //could be forced to a fixed value
    const LedEffect *curFx = fxRegistry.getCurrentEffect();
    fx["index"] = curFx->getRegistryIndex();
    fx["name"] = curFx->name();
    fx[csBroadcast] = fxBroadcastEnabled;
    auto lastFx = fx["pastEffects"].to<JsonArray>();
    fxRegistry.pastEffectsRun(lastFx); //ordered earliest to latest (current effect is the last element)
    fx[csBrightness] = stripBrightness;
    fx[csBrightnessLocked] = stripBrightnessLocked;
    fx[csAudioThreshold] = audioBumpThreshold; //current audio level threshold
    fx["totalAudioBumps"] = totalAudioBumps; //how many times (in total) have we bumped the effect due to audio level
    const auto audioHist = fx["audioHist"].to<JsonArray>();
    for (uint16_t x: maxAudio)
        audioHist.add<uint16_t>(x);
    // Time
    const auto time = doc["time"].to<JsonObject>();
    time["ntpSync"] = sysInfo->isSysStatus(SYS_STATUS_NTP);
    time["millis"] = millis(); //current time in ms
    const time_t curTime = now();
    time["sdate"] = TimeFormat::dateAsString(curTime);  //string date
    time["stime"] = TimeFormat::timeAsString(curTime);  //string time
    time["time"] = curTime; //numeric time
    const bool bDST = sysInfo->isSysStatus(SYS_STATUS_DST);
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
    for (const auto &al: scheduledAlarms) {
        auto jal = alarms.add<JsonObject>();
        jal["timeLong"] = al->value;
        jal["timeFmt"] = TimeFormat::asString(al->value);
        jal["type"] = alarmTypeToString(al->type);
        //        jal["taskPtr"] = (long)al->onEventHandler;
    }
    //System
    const auto temp = doc["temp"].to<JsonObject>();
    const auto boardTemp = temp["board"].to<JsonObject>();
    const auto cpuTemp = temp["cpu"].to<JsonObject>();
    const auto wifiTemp = temp["wifi"].to<JsonObject>();
    boardTemp["current"] = imuTempRange.current.value;
    boardTemp["max"] = imuTempRange.max.value;
    boardTemp["min"] = imuTempRange.min.value;
    cpuTemp["current"] = cpuTempRange.current.value;
    cpuTemp["max"] = cpuTempRange.max.value;
    cpuTemp["min"] = cpuTempRange.min.value;
    wifiTemp["current"] = wifiTempRange.current.value;
    wifiTemp["max"] = wifiTempRange.max.value;
    wifiTemp["min"] = wifiTempRange.min.value;
    const auto vcc = doc["vcc"].to<JsonObject>();
    vcc["current"] = lineVoltage.current.value;
    vcc["max"] = lineVoltage.max.value;
    vcc["min"] = lineVoltage.min.value;
    doc["overallStatus"] = sysInfo->getSysStatus();
#if MDNS_ENABLED==1
    doc["mdnsEnabled"] = mdns->isEnabled();
#endif
    //ISO8601 format
    //snprintf(timeBuf, 15, "P%2dDT%2dH%2dM", millis()/86400000l, (millis()/3600000l%24), (millis()/60000%60));
    //human readable format
    char timeBuf[16];
    const unsigned long upTime = millis();
    snprintf(timeBuf, 15, "%2dD %2dH %2dm", upTime / 86400000l, (upTime / 3600000l % 24), (upTime / 60000 % 60));
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

    //send it out - the size returned is http headers and response body (does not include the HTTP protocol header)
    const size_t sz = marshalJson(doc, client);
    log_info(F("Handler handleGetStatus invoked for %s, response size %zu bytes"), client.request().uri().c_str(), sz);
}

/**
 * Web request handler - PUT /fx. Method invoked by the web server; response sent to the current client awaiting response from the server
 */
void web::handlePutConfig(WebClient &client) {
    dateHeader(client);
    client.sendHeader(hdCacheControl, hdCacheJson);
    String body = client.request().body();

    //process the body - parse JSON body and react to inputs
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, body);
    if (error) {
        client.send(500, mime::mimeTable[mime::txt].mimeType, error.c_str());
        return;
    }
    JsonDocument resp;
    const auto upd = resp["updates"].to<JsonObject>();
    if (doc[csAuto].is<bool>()) {
        const bool autoAdvance = doc[csAuto].as<bool>();
        fxRegistry.autoRoll(autoAdvance);
        upd[csAuto] = autoAdvance;
    }
    if (doc[strEffect].is<uint16_t>()) {
        const auto nextFx = doc[strEffect].as<uint16_t>();
        fxRegistry.nextEffectPos(nextFx);
        upd[strEffect] = nextFx;
    }
    if (doc[csHoliday].is<String>()) {
        const auto userHoliday = doc[csHoliday].as<String>();
        paletteFactory.setHoliday(parseHoliday(&userHoliday));
        upd[csHoliday] = paletteFactory.getHoliday();
    }
    if (doc[csBrightness].is<uint8_t>()) {
        const auto br = doc[csBrightness].as<uint8_t>();
        stripBrightnessLocked = br > 0;
        stripBrightness = stripBrightnessLocked ? br : adjustStripBrightness();
        upd[csBrightness] = stripBrightness;
        upd[csBrightnessLocked] = stripBrightnessLocked;
    }
    if (doc[csAudioThreshold].is<uint16_t>()) {
        audioBumpThreshold = doc[csAudioThreshold].as<uint16_t>();
        upd[csAudioThreshold] = audioBumpThreshold;
        clearLevelHistory();
    }
    if (doc[csSleepEnabled].is<bool>()) {
        const bool sleepEnabled = doc[csSleepEnabled].as<bool>();
        fxRegistry.enableSleep(sleepEnabled);
        upd[csSleepEnabled] = sleepEnabled;
        upd["asleep"] = fxRegistry.isAsleep();
    }
    if (doc[csResetCal].is<bool>()) {
        if (const bool resetCal = doc[csResetCal].as<bool>()) {
            calibTempMeasurements.reset();
            calibCpuTemp.reset();
            cpuTempRange.reset();
            SyncFsImpl.remove(calibFileName);
            upd[csResetCal] = resetCal;
        }
    }
    if (doc[csBroadcast].is<bool>()) {
        const bool syncMode = doc[csBroadcast].as<bool>();
        const bool masterEnabled = syncMode != fxBroadcastEnabled && syncMode;
        fxBroadcastEnabled = syncMode; //we need this enabled before we post the event, if we're doing that
        if (masterEnabled)
            postFxChangeEvent(fxRegistry.curEffectPos()); //we've just enabled broadcasting (this board is a master), issue a sync event to all other boards
    }
    log_info(F("FX: Current config updated effect %hu, autoswitch %s, holiday %s, brightness %hu, brightness adjustment %s"),
        fxRegistry.curEffectPos(), StringUtils::asString(fxRegistry.isAutoRoll()),
        holidayToString(paletteFactory.getHoliday()),
        stripBrightness, stripBrightnessLocked?"fixed":"automatic");

    //main status and headers
    resp["status"] = true;

    contentDispositionHeader(client, statusJsonFilename);
    //send it out
    const size_t sz = marshalJson(resp, client);
    log_info(F("Handler handlePutConfig invoked for %s, response size %zu bytes"), client.request().uri().c_str(), sz);
}

/**
 * Web request handler - GET /tasks.json. Method invoked by the web server; response sent to the current client awaiting response from the server
 */
void web::handleGetTasks(WebClient &client) {
    dateHeader(client);
    contentDispositionHeader(client, tasksJsonFilename);
    client.sendHeader(hdCacheControl, hdCacheJson);

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
    doc["boardName"] = sysInfo->getBoardName();
    doc["boardUid"] = sysInfo->getBoardId();
    doc["fwVersion"] = sysInfo->getBuildVersion();
    doc["fwBranch"] = sysInfo->getScmBranch();
    doc["buildTime"] = sysInfo->getBuildTime();

    //send it out
    const size_t sz = marshalJson(doc, client);
    log_info(F("Handler handleGetStatus invoked for %s, response size %zu bytes"), client.request().uri().c_str(), sz);
}

/**
 * Special web request handler for resources not found on this server
 */
void web::handleNotFound(WebClient &client) {
    const size_t sz = client.send(404, mime::mimeTable[mime::txt].mimeType, msgRequestNotMapped);
    log_info(F("Handler handleNotFound invoked for %s, response size %zu bytes"), client.request().uri().c_str(), sz);
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
            fwData->auth = req.header("X-Token").equals("KlFpc1dAdFd0eDRXdkVSZg");
            fwData->checkSum = req.header("X-Check");
            fwData->checkSum.toLowerCase();
            fwData->fileName = csFWImageFilename;
            if (fwData->auth) {
                //create a file for the incoming data
                if (SyncFsImpl.exists(fwData->fileName.c_str()))
                    SyncFsImpl.remove(fwData->fileName.c_str());
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
            if (SyncFsImpl.exists(fd->fileName.c_str()))
                SyncFsImpl.remove(fd->fileName.c_str());
            delete fd;
        }
        break;
        default: break;
    }
}

/**
 * Convenience no-op handler. Can also be replaced by a lambda function \code [](WebClient &) { }\endcode
 * @param client web client
 */
void noop(WebClient &client) {}

/**
 * Configures the web server with specific dynamic and static request handlers
 * This method can be called again upon WiFi reconnecting
 */
void web::server_setup() {
    if (!server_handlers_configured) {
        log_info(F("Starting Web server setup"));
        server.setServerAgent(serverAgent);
        server.serveStatic("/", SyncFsImpl, "/status/", &inFlashResources, hdCacheStatic);
        server.serveStatic("/config.json", SyncFsImpl, "/status/sysconfig.json", nullptr, hdCacheJson);
        server.on("/status.json", HTTP_GET, handleGetStatus);
        server.on("/fx", HTTP_PUT, handlePutConfig);
        server.on("/tasks.json", HTTP_GET, handleGetTasks);
        server.on("/fw", HTTP_POST, noop, handleFWImageUpload);
        server.onNotFound(handleNotFound);
        server.enableDelay(false); //the task that runs the web-server also runs other services, do not want to introduce unnecessary delays
        server_handlers_configured = true;
        log_info(F("Completed Web server setup"));
    }
    server.collectHeaders("Host", "Accept", "Referer", "User-Agent", "X-Token", "X-Check");
    server.begin(serverPort);
    log_info(F("Web server started"));
}

/**
 * Web Server client handling - one at a time
 */
void web::webserver() {
    server.handleClient();
#if MDNS_ENABLED==1
    EVERY_N_SECONDS(3) {
        if (server.state() == HTTPServer::IDLE)
            mdnsStatus = mdns->process();
    }
#endif
}
