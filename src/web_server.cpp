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

static constexpr auto http200Status PROGMEM = "HTTP/1.1 200 OK";
// static constexpr auto http303Status PROGMEM = "HTTP/1.1 303 See Other";
static constexpr auto http404Status PROGMEM = "HTTP/1.1 404 Not Found";
static constexpr auto http500Status PROGMEM = "HTTP/1.1 500 Internal Server Error";

static constexpr auto hdHtml PROGMEM = R"===(Content-type: text/html
Server: rp2040-luca/1.0.0)===";

static constexpr auto hdCss PROGMEM = R"===(Content-type: text/css
Server: rp2040-luca/1.0.0
Cache-Control: public, max-age=2592000, immutable)===";

static constexpr auto hdJavascript PROGMEM = R"===(Content-type: text/javascript
Server: rp2040-luca/1.0.0
Cache-Control: public, max-age=2592000, immutable)===";

static constexpr auto hdJson PROGMEM = R"===(Content-type: application/json
Server: rp2040-luca/1.0.0
Cache-Control: no-cache, no-store)===";

static constexpr auto hdCacheControl PROGMEM = "Cache-Control";
static constexpr auto hdCacheStatic PROGMEM = "public, max-age=2592000, immutable";
static constexpr auto hdCacheJson PROGMEM = "no-cache, no-store";
static constexpr auto serverAgent PROGMEM = "rp2040-luca/1.0.0";

using namespace web;
// using namespace colTheme;

// static constexpr auto hdRootLocation PROGMEM = "Location: /";
static constexpr auto hdConClose PROGMEM = "Connection: close";
static constexpr auto hdFmtContentLength PROGMEM = "Content-Length: %d";
static constexpr auto hdFmtDate PROGMEM = "%4d-%02d-%02d %02d:%02d:%02d CST";
static constexpr auto hdFmtContentDisposition PROGMEM = "inline; filename=\"%s\"";
static constexpr auto msgRequestNotMapped PROGMEM = "URI not mapped to a handler on this server";
static constexpr auto configJsonFilename PROGMEM = "config.json";
static constexpr auto statusJsonFilename PROGMEM = "status.json";
static constexpr auto tasksJsonFilename PROGMEM = "tasks.json";

WebServer web::server(80);
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
static const std::map<std::string, const char*> inFlashResources PROGMEM = {
    {"/pixel.css", pixel_css},
    {"/pixel.js", pixel_js},
    {"/index.html", index_html},
    {"/", index_html},
    {"/stats.css", stats_css},
    {"/stats.js", stats_js},
    {"/stats.html", stats_html}
};

void dateHeader() {
    const time_t curTime = now();
    const int szBuf = snprintf(nullptr, 0, hdFmtDate, year(curTime), month(curTime), day(curTime), hour(curTime), minute(curTime), second(curTime)) + 1;
    char buf[szBuf];
    snprintf(buf, szBuf, hdFmtDate, year(curTime), month(curTime), day(curTime), hour(curTime), minute(curTime), second(curTime));
    server.sendHeader(F("Date"), buf);
}

void contentDispositionHeader(const char *fname) {
    const int szBuf = snprintf(nullptr, 0, hdFmtContentDisposition, fname) + 1;
    char buf[szBuf];
    snprintf(buf, szBuf, hdFmtContentDisposition, fname);
    server.sendHeader(F("Content-Disposition"), buf);
}

size_t web::marshalJson(const JsonDocument &doc, WebServer &srv) {
    //send it out
    const auto buf = new String();
    buf->reserve(5120);     // deemed enough for all/most json docs in this app (largest is the config file at 4800 bytes)
    serializeJson(doc, *buf);
    const size_t sz = srv.send(200, mime::mimeTable[mime::json].mimeType, buf->c_str());
    delete buf;
    return sz;
}

void web::handleGetStatus() {
    dateHeader();
    contentDispositionHeader(statusJsonFilename);
    server.sendHeader(hdCacheControl, hdCacheJson);

    // response body
    JsonDocument doc;
    // WiFi
    const auto wifi = doc["wifi"].to<JsonObject>();
    wifi["IP"] = sysInfo->getIpAddress();         //IP Address
    const int32_t rssi = WiFi.RSSI();
    wifi["bars"] = barSignalLevel(rssi);  //Wi-Fi signal level
    wifi["rssi"] = rssi;
    // Fx
    const auto fx = doc["fx"].to<JsonObject>();
    fx[csAuto] = fxRegistry.isAutoRoll();
    fx[csSleepEnabled] = fxRegistry.isSleepEnabled();
    fx["asleep"] = fxRegistry.isAsleep();
    fx["autoTheme"] = paletteFactory.isAuto();
    fx["theme"] = holidayToString(paletteFactory.getHoliday());   //could be forced to a fixed value
    const LedEffect *curFx = fxRegistry.getCurrentEffect();
    fx["index"] = curFx->getRegistryIndex();
    fx["name"] = curFx->name();
    fx[csBroadcast] = fxBroadcastEnabled;
    auto lastFx = fx["pastEffects"].to<JsonArray>();
    fxRegistry.pastEffectsRun(lastFx);                   //ordered earliest to latest (current effect is the last element)
    fx[csBrightness] = stripBrightness;
    fx[csBrightnessLocked] = stripBrightnessLocked;
    fx[csAudioThreshold] = audioBumpThreshold;              //current audio level threshold
    fx["totalAudioBumps"] = totalAudioBumps;                //how many times (in total) have we bumped the effect due to audio level
    const auto audioHist = fx["audioHist"].to<JsonArray>();
    for (uint16_t x : maxAudio)
        audioHist.add<uint16_t>(x);
    // Time
    const auto time = doc["time"].to<JsonObject>();
    time["ntpSync"] = timeStatus();
    time["millis"] = millis();           //current time in ms
    char timeBuf[21];
    const time_t curTime = now();
    formatDate(timeBuf, curTime);
    time["date"] = timeBuf;
    formatTime(timeBuf, curTime);
    time["time"] = timeBuf;
    time["dst"] = sysInfo->isSysStatus(SYS_STATUS_DST);
    time[csHoliday] = holidayToString(currentHoliday());      //time derived holiday
    time["syncSize"] = timeSyncs.size();
    time["averageDrift"] = getAverageTimeDrift();
    time["lastDrift"] = getLastTimeDrift();
    time["totalDrift"] = getTotalDrift();
    const auto syncs = time["syncs"].to<JsonArray>();
    for (const auto &[localMillis, unixSeconds] : timeSyncs) {
        auto jts = syncs.add<JsonObject>();
        jts["localMillis"] = localMillis;
        jts["unixSeconds"] = unixSeconds;
    }
    const auto alarms = time["alarms"].to<JsonArray>();
    for (const auto &al : scheduledAlarms) {
        auto jal = alarms.add<JsonObject>();
        jal["timeLong"] = al->value;
        formatDateTime(timeBuf, al->value);
        jal["timeFmt"] = timeBuf;
        jal["type"] = alarmTypeToString(al->type);
//        jal["taskPtr"] = (long)al->onEventHandler;
    }
    //System
    doc["boardTemp"] = imuTempRange.current.value;
    doc["chipTemp"] = cpuTempRange.current.value;
    doc["wifiTemp"] = WiFi.getTemperature();
    doc["vcc"] = lineVoltage.current.value;
    doc["minVcc"] = lineVoltage.min.value;
    doc["maxVcc"] = lineVoltage.max.value;
    doc["boardMinTemp"] = imuTempRange.min.value;
    doc["boardMaxTemp"] = imuTempRange.max.value;
    doc["overallStatus"] = sysInfo->getSysStatus();
    //ISO8601 format
    //snprintf(timeBuf, 15, "P%2dDT%2dH%2dM", millis()/86400000l, (millis()/3600000l%24), (millis()/60000%60));
    //human readable format
    const unsigned long upTime = millis();
    snprintf(timeBuf, 15, "%2dD %2dH %2dm", upTime/86400000l, (upTime/3600000l%24), (upTime/60000%60));
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

    //send it out
    const size_t sz = marshalJson(doc, server);
    Log.info(F("Handler handleGetStatus invoked for %s, response completed %zu bytes"), server.uri().c_str(), sz);
}

void web::handlePutConfig() {
    dateHeader();
    server.sendHeader(hdCacheControl, hdCacheJson);
    // String body = server2.arg(reqBodyArgName); OR ?
    HTTPRaw &raw = server.raw();
    String body(raw.buf, raw.currentSize);

    //process the body - parse JSON body and react to inputs
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, body);
    if (error) {
        server.send(500, mime::mimeTable[mime::txt].mimeType, error.c_str());
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
        const auto nextFx = doc[strEffect].as<uint16_t >();
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
            removeFile(calibFileName);
            upd[csResetCal] = resetCal;
        }
    }
    if (doc[csBroadcast].is<bool>()) {
        const bool syncMode = doc[csBroadcast].as<bool>();
        const bool masterEnabled = syncMode != fxBroadcastEnabled && syncMode;
        fxBroadcastEnabled = syncMode;  //we need this enabled before we post the event, if we're doing that
        if (masterEnabled)
            postFxChangeEvent(fxRegistry.curEffectPos());   //we've just enabled broadcasting (this board is a master), issue a sync event to all other boards
    }
    Log.info(F("FX: Current config updated effect %hu, autoswitch %s, holiday %s, brightness %hu, brightness adjustment %s"),
               fxRegistry.curEffectPos(), StringUtils::asString(fxRegistry.isAutoRoll()), holidayToString(paletteFactory.getHoliday()),
               stripBrightness, stripBrightnessLocked?"fixed":"automatic");

    //main status and headers
    resp["status"] = true;

    contentDispositionHeader(statusJsonFilename);
    //send it out
    const size_t sz = marshalJson(resp, server);
    Log.info(F("Handler handlePutConfig invoked for %s, response completed %zu bytes"), server.uri().c_str(), sz);
}

void web::handleGetTasks() {
    dateHeader();
    contentDispositionHeader(tasksJsonFilename);
    server.sendHeader(hdCacheControl, hdCacheJson);

    // response body
    JsonDocument doc;
    doc["cycles32"] = rp2040.getCycleCount();
    doc["cycles64"] = rp2040.getCycleCount64();
    doc["millis"] = millis();
    const time_t curTime = now();
    char timeBuf[21];
    formatDate(timeBuf, curTime);
    doc["date"] = timeBuf;
    formatTime(timeBuf, curTime);
    doc["time"] = timeBuf;
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
    const size_t sz = marshalJson(doc, server);
    Log.info(F("Handler handleGetStatus invoked for %s, response completed %zu bytes"), server.uri().c_str(), sz);
}

void web::handleNotFound() {
    const size_t sz = server.send(404, mime::mimeTable[mime::txt].mimeType, msgRequestNotMapped);
    Log.info(F("Handler handleNotFound invoked for %s, response completed %zu bytes"), server.uri().c_str(), sz);
}

void web::server_setup() {
    if (!server_handlers_configured) {
        server.setServerAgent(serverAgent);
        server.serveStatic("/", LittleFS, "/status/", &inFlashResources, hdCacheStatic);
        server.serveStatic("/config.json", LittleFS, "/status/sysconfig.json", nullptr, hdCacheJson);
        server.on("/status.json", HTTP_GET, handleGetStatus);
        server.on("/fx", HTTP_PUT, handlePutConfig);
        server.on("/tasks.json", HTTP_GET, handleGetTasks);
        server.onNotFound(handleNotFound);
        server_handlers_configured = true;
    }
    server.begin();
    Log.info(F("Web server started"));
}

void web::webserver() {
    if (WiFi.status() != WL_CONNECTED)
        return;
    server.handleClient();
}
