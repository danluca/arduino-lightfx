//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
#include "web_server.h"
#include "log.h"
#include "timeutil.h"
#include "sysinfo.h"
#include "diag.h"
#include "broadcast.h"
#include "StreamUtils.h"

static const char http200Status[] PROGMEM = "HTTP/1.1 200 OK";
static const char http303Status[] PROGMEM = "HTTP/1.1 303 See Other";
static const char http404Status[] PROGMEM = "HTTP/1.1 404 Not Found";
static const char http500Status[] PROGMEM = "HTTP/1.1 500 Internal Server Error";

static const char hdHtml[] PROGMEM = R"===(Content-type: text/html
Server: rp2040-luca/1.0.0)===";

static const char hdCss[] PROGMEM = R"===(Content-type: text/css
Server: rp2040-luca/1.0.0
Cache-Control: public, max-age=2592000, immutable)===";

static const char hdJavascript[] PROGMEM = R"===(Content-type: text/javascript
Server: rp2040-luca/1.0.0
Cache-Control: public, max-age=2592000, immutable)===";

static const char hdJson[] PROGMEM = R"===(Content-type: application/json
Server: rp2040-luca/1.0.0
Cache-Control: no-cache, no-store)===";

using namespace web;
using namespace colTheme;

static const char hdRootLocation[] PROGMEM = "Location: /";
static const char hdConClose[] PROGMEM = "Connection: close";
static const char hdFmtContentLength[] PROGMEM = "Content-Length: %d";
static const char hdFmtDate[] PROGMEM = "Date: %4d-%02d-%02d %02d:%02d:%02d CST";
static const char hdFmtContentDisposition[] PROGMEM = "Content-Disposition: inline; filename=\"%s\"";
static const char msgRequestNotMapped[] PROGMEM = "URI not mapped to a handler on this server";
static const char configJsonFilename[] PROGMEM = "config.json";
static const char statusJsonFilename[] PROGMEM = "status.json";

/**
 * Web handler mappings - static in nature and stored in flash
 * The mapping consists of a string key - <code>Http_Method URI</code> associated with the function pointer that
 * handles the respective requests.
 * If more request mappings are needed - add them here, following the pattern
 */
static const std::map<std::string, reqHandler> webMappings PROGMEM = {
        {"^GET /config\\.json$",  handleGetConfig},
        {"^GET /status\\.json$",  handleGetStatus},
        {"^GET /\\w+\\.css$",     handleGetCss},
        {"^GET /[\\w.]+\\.js$",   handleGetJs},
        {"^GET /\\w+\\.html$",    handleGetHtml},
        {"^GET /$",               handleGetRoot},
        {"^PUT /fx$",             handlePutConfig}
};

rtos::Mutex wifiMutex;
// global server object - through WiFi module
WiFiServer server(80);
//size of the buffer for buffering the response
static const uint16_t WEB_BUFFER_SIZE = 1024;

/**
 * Start the server
 */
void server_setup() {
    server.begin();
}

/**
 * Dispatch incoming requests to their handlers
 */
void webserver() {
    ScopedMutexLock lock(wifiMutex);
    web::dispatch();
}

/**
 * Utility to send the Date http header with current time
 * @param client the web client to write to
 * @return number of bytes written to the client
 */
size_t writeDateHeader(WiFiClient *client) {
    time_t curTime = now();
    int szBuf = snprintf(nullptr, 0, hdFmtDate, year(curTime), month(curTime), day(curTime), hour(curTime), minute(curTime), second(curTime)) + 1;
    char buf[szBuf];
    sprintf(buf, hdFmtDate, year(curTime), month(curTime), day(curTime), hour(curTime), minute(curTime), second(curTime));
    return client->println(buf);
}

/**
 * Utility to send the Content Disposition http header with a file name
 * @param client the web client to write to
 * @param fname the file name
 * @return number of bytes written to the client
 */
size_t writeFilenameHeader(WiFiClient *client, const char *fname) {
    int szBuf = snprintf(nullptr, 0, hdFmtContentDisposition, fname) + 1;
    char buf[szBuf];
    sprintf(buf, hdFmtContentDisposition, fname);
    return client->println(buf);
}

/**
 * Utility to send the Content Length http header with response body length
 * @param client the web client to write to
 * @param szContent size of the response body
 * @return number of bytes written to the client
 */
size_t writeContentLengthHeader(WiFiClient *client, uint32_t szContent) {
    int szBuf = snprintf(nullptr, 0, hdFmtContentLength, szContent) + 1;
    char buf[szBuf];
    sprintf(buf, hdFmtContentLength, szContent);
    return client->println(buf);
}

/**
 * Utility to write large text contents (stored in PROGMEM) using buffering. It has been noted the WiFiClient chokes for strings larger than 4k
 * @param client the web client to write to
 * @param src source textual content to write (can also be stored in PROGMEM flash)
 * @param srcSize number of bytes to write - this is arguably same as <code>strLen(src)</code> (where src is a null-terminated string). However,
 * saving the trouble of traversing the char array one more time for determining the length. It is needed before-hand to write the content length header.
 * @return number of bytes written to the client
 */
size_t writeLargeP(WiFiClient *client, const char *src, size_t srcSize) {
    // buffering stream implementation, generic for regular char arrays
    // note these buffers expect to write (repeatedly) chunks of data less than their size/capacity, and this is when buffering is engaged; the implementation does not chunk on its own
    // if attempting to write a data chunk that is larger than buffer capacity, it will go straight to the client delegate skipping the buffer
//    WriteBufferingStream bufferingStream(*client, WEB_BUFFER_SIZE);
//    size_t res = 0;
//    while (srcSize > 0) {
//        size_t sz = bufferingStream.write(src, min(srcSize, WEB_BUFFER_SIZE-2));  //engage the buffer
//        src += sz;
//        srcSize -= sz;
//        res += sz;
//    }
//    bufferingStream.flush();

    char buf[WEB_BUFFER_SIZE];
    size_t res = 0;
    while (srcSize > 0) {
        size_t sz = min(srcSize, WEB_BUFFER_SIZE);
        memcpy_P(buf, src, sz);     //for RP2040 this is the same as regular memory copy, no difference for copying from flash (and we could have written to WiFi client straight from source, could have saved a local buffer)
        sz = client->write(buf, sz);
        src += sz;
        srcSize -= sz;  //because sz is min between srcSize and buffer size, when srcSize lowers below buffer size, sz = srcSize and this statement brings srcSize to 0
        res += sz;
    }
    return res;
}

/**
 * Performance optimized JSON document serialized into WiFi client
 * Note: ArduinoJson's serializeJson using WiFi client (a Print instance) is very inefficient - writes one byte at a time, which in turn
 * become one network frame. This implementation buffers the content into a String and then writes the entire String
 * @param source the JSON document to serialize
 * @param client WiFi client to write into
 * @return number of bytes written
 * @see https://github.com/bblanchon/ArduinoStreamUtils
 */
size_t web::transmitJsonDocument(JsonVariantConst source, WiFiClient *client) {
    WriteBufferingStream bufferingStream(*client, WEB_BUFFER_SIZE);
    size_t sz = serializeJson(source, bufferingStream);
    bufferingStream.flush();

    // custom implementation with String buffer on the heap
//    size_t sz = measureJson(source);
//    auto buf = new String();
//    buf->reserve(sz);
//
//    serializeJson(source, *buf);
//    if (sz > WEB_BUFFER_SIZE)
//        sz = writeLargeP(client, buf->c_str(), buf->length());
//    else
//        sz = client->print(*buf);
//    delete buf;
    return sz;
}

/**
 * Handles <code>GET /config.json</code> - responds with JSON document containing effects configuration details
 * <p>Must comply with the <code>reqHandler</code> function pointer signature</p>
 * @param client the web client to respond to
 * @param uri URI invoked
 * @param hd request headers
 * @param bdy request body - empty (this is a GET request)
 * @return number of bytes sent to the client
 */
size_t web::handleGetConfig(WiFiClient *client, String *uri, String *hd, String *bdy) {
    //main status and headers
    size_t sz = client->println(http200Status);
    sz += client->println(hdJson);
    sz += client->println(hdConClose);
    sz += writeDateHeader(client);
    sz += writeFilenameHeader(client, configJsonFilename);
    sz += client->println();    //done with headers

    // response body
    JsonDocument doc;
    char buf[20];

    doc["boardName"] = sysInfo->getBoardName();
    doc["boardUid"] = sysInfo->getBoardId();
    doc["fwVersion"] = sysInfo->getBuildVersion();
    doc["fwBranch"] = sysInfo->getScmBranch();
    doc["buildTime"] = sysInfo->getBuildTime();
    doc["watchdogRebootsCount"] = sysInfo->watchdogReboots().size();
    doc["cleanBoot"] = sysInfo->isCleanBoot();
    if (!sysInfo->watchdogReboots().empty()) {
        formatDateTime(buf, sysInfo->watchdogReboots().back());
        doc["lastWatchdogReboot"] = buf;
    }
    doc["curEffect"] = String(fxRegistry.curEffectPos());
    doc["auto"] = fxRegistry.isAutoRoll();
    doc[csSleepEnabled] = fxRegistry.isSleepEnabled();
    doc["curEffectName"] = fxRegistry.getCurrentEffect()->name();
    doc["holiday"] = holidayToString(paletteFactory.getHoliday());
    JsonArray hldList = doc["holidayList"].to<JsonArray>();
    for (uint8_t hi = None; hi <= NewYear; hi++)
        hldList.add(holidayToString(static_cast<Holiday>(hi)));
    formatDateTime(buf, now());
    doc["currentTime"] = buf;
    bool bDST = sysInfo->isSysStatus(SYS_STATUS_DST);
    doc["currentOffset"] = bDST ? CDT_OFFSET_SECONDS : CST_OFFSET_SECONDS;
    doc["dst"] = bDST;
    doc["MAC"] = sysInfo->getMacAddress();
    JsonArray fxArray = doc["fx"].to<JsonArray>();
    fxRegistry.describeConfig(fxArray);
    //send it out
    sz += transmitJsonDocument(doc, client);
    sz += client->println();

#ifndef DISABLE_LOGGING
    Log.infoln(F("Handler handleGetConfig invoked for %s"), uri->c_str());
#endif
    return sz;
}

/**
 * Handles <code>GET /*.css</code> - responds with the sole CSS stylesheet, pixel.css
 * <p>Must comply with the <code>reqHandler</code> function pointer signature</p>
 * @param client the web client to respond to
 * @param uri URI invoked
 * @param hd request headers
 * @param bdy request body - empty (this is a GET request)
 * @return number of bytes sent to the client
 */
size_t web::handleGetCss(WiFiClient *client, String *uri, String *hd, String *bdy) {
    //main status and headers
    size_t sz = client->println(http200Status);
    sz += client->println(hdCss);
    sz += writeDateHeader(client);
    sz += client->println(hdConClose);
    size_t cssLen = strlen(pixel_css);
    sz += writeContentLengthHeader(client, cssLen);
    sz += client->println();    //done with headers

    // response body
    sz += writeLargeP(client, pixel_css, cssLen);
    sz += client->println();

#ifndef DISABLE_LOGGING
    Log.infoln(F("Handler handleGetCss invoked for %s"), uri->c_str());
#endif
    return sz;
}

/**
 * Handles <code>GET /*.js</code> - responds with JS files - one of three options: jquery, jquery-ui, pixel.js (last one is default)
 * <p>Must comply with the <code>reqHandler</code> function pointer signature</p>
 * @param client the web client to respond to
 * @param uri URI invoked
 * @param hd request headers
 * @param bdy request body - empty (this is a GET request)
 * @return number of bytes sent to the client
 */
size_t web::handleGetJs(WiFiClient *client, String *uri, String *hd, String *bdy) {
    //main status and headers
    size_t sz = client->println(http200Status);
    sz += client->println(hdJavascript);
    sz += writeDateHeader(client);
    sz += client->println(hdConClose);

    // figure out which JS source we need
    const char* src = pixel_js;
//    if (uri->endsWith("jquery.min.js"))
//        src = jquery_min_js;
    // determine size
    size_t jsLen = strlen(src);
    sz += writeContentLengthHeader(client, jsLen);
    sz += client->println();    //done with headers

    // response body
    sz += writeLargeP(client, src, jsLen);
    sz += client->println();

#ifndef DISABLE_LOGGING
    Log.infoln(F("Handler handleGetJs invoked for %s"), uri->c_str());
#endif
    return sz;
}

/**
 * Handles <code>GET /index.html</code> - responds with main HTML page (index.html)
 * <p>Must comply with the <code>reqHandler</code> function pointer signature</p>
 * @param client the web client to respond to
 * @param uri URI invoked
 * @param hd request headers
 * @param bdy request body - empty (this is a GET request)
 * @return number of bytes sent to the client
 */
size_t web::handleGetHtml(WiFiClient *client, String *uri, String *hd, String *bdy) {
    //main status and headers
    size_t sz = client->println(http200Status);
    sz += client->println(hdHtml);
    sz += writeDateHeader(client);
    sz += client->println(hdConClose);

    // determine size
    size_t szHtml = strlen(index_html);
    sz += writeContentLengthHeader(client, szHtml);
    sz += client->println();    //done with headers

    // response body
    sz += writeLargeP(client, index_html, szHtml);
    sz += client->println();

#ifndef DISABLE_LOGGING
    Log.infoln(F("Handler handleGetHtml invoked for %s"), uri->c_str());
#endif
    return sz;
}

/**
 * Handles <code>GET /</code> - responds with main HTML page (index.html)
 * <p>Must comply with the <code>reqHandler</code> function pointer signature</p>
 * @param client the web client to respond to
 * @param uri URI invoked
 * @param hd request headers
 * @param bdy request body - empty (this is a GET request)
 * @return number of bytes sent to the client
 * @see web::handleGetHtml
 */
size_t web::handleGetRoot(WiFiClient *client, String *uri, String *hd, String *bdy) {
    return handleGetHtml(client, uri, hd, bdy);
}

/**
 * Handles <code>GET /status.json</code> - responds with JSON document containing current status of the system: WiFi, current effect, time
 * <p>Must comply with the <code>reqHandler</code> function pointer signature</p>
 * @param client the web client to respond to
 * @param uri URI invoked
 * @param hd request headers
 * @param bdy request body - empty (this is a GET request)
 * @return number of bytes sent to the client
 */
size_t web::handleGetStatus(WiFiClient *client, String *uri, String *hd, String *bdy) {
    //main status and headers
    size_t sz = client->println(http200Status);
    sz += client->println(hdJson);
    sz += writeDateHeader(client);
    sz += writeFilenameHeader(client, statusJsonFilename);
    sz += client->println(hdConClose);
    sz += client->println();    //done with headers

    // response body
    JsonDocument doc;
    // WiFi
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["IP"] = sysInfo->getIpAddress();         //IP Address
    int32_t rssi = WiFi.RSSI();
    wifi["bars"] = barSignalLevel(rssi);  //Wi-Fi signal level
    wifi["rssi"] = rssi;
    wifi["curVersion"] = sysInfo->getWiFiFwVersion();
    wifi["latestVersion"] = WIFI_FIRMWARE_LATEST_VERSION;
    // Fx
    JsonObject fx = doc["fx"].to<JsonObject>();
    fx["count"] = fxRegistry.size();
    fx[csAuto] = fxRegistry.isAutoRoll();
    fx[csSleepEnabled] = fxRegistry.isSleepEnabled();
    fx["asleep"] = fxRegistry.isAsleep();
    fx[csHoliday] = holidayToString(paletteFactory.getHoliday());   //could be forced to a fixed value
    const LedEffect *curFx = fxRegistry.getCurrentEffect();
    fx["index"] = curFx->getRegistryIndex();
    fx["name"] = curFx->name();
    fx[csBroadcast] = fxBroadcastEnabled;
    JsonArray lastFx = fx["pastEffects"].to<JsonArray>();
    fxRegistry.pastEffectsRun(lastFx);                   //ordered earliest to latest (current effect is the last element)
    fx[csBrightness] = stripBrightness;
    fx[csBrightnessLocked] = stripBrightnessLocked;
    fx[csAudioThreshold] = audioBumpThreshold;              //current audio level threshold
    fx["totalAudioBumps"] = totalAudioBumps;                //how many times (in total) have we bumped the effect due to audio level
    JsonArray audioHist = fx["audioHist"].to<JsonArray>();
    for (uint16_t x : maxAudio)
        audioHist.add(x);
    // Time
    JsonObject time = doc["time"].to<JsonObject>();
    time["ntpSync"] = timeStatus();
    time["millis"] = millis();           //current time in ms
    char timeBuf[21];
    time_t curTime = now();
    formatDate(timeBuf, curTime);
    time["date"] = timeBuf;
    formatTime(timeBuf, curTime);
    time["time"] = timeBuf;
    time["dst"] = sysInfo->isSysStatus(SYS_STATUS_DST);
    time["holiday"] = holidayToString(currentHoliday());      //time derived holiday
    time["syncSize"] = timeSyncs.size();
    time["averageDrift"] = getAverageTimeDrift();
    time["lastDrift"] = getLastTimeDrift();
    time["totalDrift"] = getTotalDrift();
    JsonArray alarms = time["alarms"].to<JsonArray>();
    for (const auto &al : scheduledAlarms) {
        JsonObject jal = alarms.add<JsonObject>();
        jal["timeLong"] = al->value;
        formatDateTime(timeBuf, al->value);
        jal["timeFmt"] = timeBuf;
        jal["type"] = alarmTypeToString(al->type);
//        jal["taskPtr"] = (long)al->onEventHandler;
    }
    //System
    snprintf(timeBuf, 9, "%2d.%02d.%02d", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
    doc["mbedVersion"] = timeBuf;
    doc["boardTemp"] = imuTempRange.current.value;
    doc["chipTemp"] = cpuTempRange.current.value;
    doc["vcc"] = lineVoltage.current.value;
    doc["minVcc"] = lineVoltage.min.value;
    doc["maxVcc"] = lineVoltage.max.value;
    doc["boardMinTemp"] = imuTempRange.min.value;
    doc["boardMaxTemp"] = imuTempRange.max.value;
    doc["overallStatus"] = sysInfo->getSysStatus();
    //ISO8601 format
    //snprintf(timeBuf, 15, "P%2dDT%2dH%2dM", millis()/86400000l, (millis()/3600000l%24), (millis()/60000%60));
    //human readable format
    snprintf(timeBuf, 15, "%2dD %2dH %2dm", millis()/86400000l, (millis()/3600000l%24), (millis()/60000%60));
    doc["upTime"] = timeBuf;
    JsonObject cpuTempCal = doc["cpuTempCal"].to<JsonObject>();
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
    sz += transmitJsonDocument(doc, client);
    sz += client->println();

#ifndef DISABLE_LOGGING
    Log.infoln(F("Handler handleGetStatus invoked for %s"), uri->c_str());
#endif
    return sz;
}

/**
 * Handles <code>PUT /fx</code> - updates the effect(s) configuration
 * <p>Must comply with the <code>reqHandler</code> function pointer signature</p>
 * @param client the web client to respond to
 * @param uri URI invoked
 * @param hd request headers
 * @param bdy request body - JSON of the update request
 * @return number of bytes sent to the client
 */
size_t web::handlePutConfig(WiFiClient *client, String *uri, String *hd, String *bdy) {
    //process the body - parse JSON body and react to inputs
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, *bdy);
    if (error)
        return handleInternalError(client, uri, error.c_str());

    JsonDocument resp;
    const char strEffect[] = "effect";
    JsonObject upd = resp["updates"].to<JsonObject>();
    if (doc.containsKey(csAuto)) {
        bool autoAdvance = doc[csAuto].as<bool>();
        fxRegistry.autoRoll(autoAdvance);
        upd[csAuto] = autoAdvance;
    }
    if (doc.containsKey(strEffect)) {
        auto nextFx = doc[strEffect].as<uint16_t >();
        fxRegistry.nextEffectPos(nextFx);
        upd[strEffect] = nextFx;
    }
    if (doc.containsKey(csHoliday)) {
        String userHoliday = doc[csHoliday].as<String>();
        paletteFactory.setHoliday(parseHoliday(&userHoliday));
        upd[csHoliday] = paletteFactory.adjustHoliday();
    }
    if (doc.containsKey(csBrightness)) {
        auto br = doc[csBrightness].as<uint8_t>();
        stripBrightnessLocked = br > 0;
        stripBrightness = stripBrightnessLocked ? br : adjustStripBrightness();
        upd[csBrightness] = stripBrightness;
        upd[csBrightnessLocked] = stripBrightnessLocked;
    }
    if (doc.containsKey(csAudioThreshold)) {
        audioBumpThreshold = doc[csAudioThreshold].as<uint16_t>();
        upd[csAudioThreshold] = audioBumpThreshold;
        clearLevelHistory();
    }
    if (doc.containsKey(csSleepEnabled)) {
        bool sleepEnabled = doc[csSleepEnabled].as<bool>();
        fxRegistry.enableSleep(sleepEnabled);
        upd[csSleepEnabled] = sleepEnabled;
        upd["asleep"] = fxRegistry.isAsleep();
    }
    const char csResetCal[] = "resetTempCal";
    if (!doc[csResetCal].isNull()) {
        bool resetCal = doc[csResetCal].as<bool>();
        if (resetCal) {
            calibTempMeasurements.reset();
            calibCpuTemp.reset();
            cpuTempRange.reset();
            removeFile(calibFileName);
            upd[csResetCal] = resetCal;
        }
    }
    if (!doc[csBroadcast].isNull()) {
        bool syncMode = doc[csBroadcast].as<bool>();
        bool masterEnabled = syncMode != fxBroadcastEnabled && syncMode;
        fxBroadcastEnabled = syncMode;  //we need this enabled before we post the event, if we're doing that
        if (masterEnabled)
            postFxChangeEvent(fxRegistry.curEffectPos());   //we've just enabled broadcasting (this board is a master), issue a sync event to all other boards
    }
#ifndef DISABLE_LOGGING
    Log.infoln(F("FX: Current running effect updated to %u, autoswitch %T, holiday %s, brightness %u, brightness adjustment %s"),
               fxRegistry.curEffectPos(), fxRegistry.isAutoRoll(), holidayToString(paletteFactory.getHoliday()),
               stripBrightness, stripBrightnessLocked?"fixed":"automatic");
#endif

    //main status and headers
    resp["status"] = true;

    size_t sz = client->println(http200Status);
    sz += client->println(hdJson);
    sz += writeDateHeader(client);
    sz += client->println(hdConClose);
    sz += client->println();    //done with headers

    sz += transmitJsonDocument(resp, client);
    sz += client->println();

#ifndef DISABLE_LOGGING
    Log.infoln(F("Handler handlePutConfig invoked for %s"), uri->c_str());
#endif
    return sz;
}

/**
 * Generic internal error handler - responds with JSON message
 * @param client the web client to respond to
 * @param uri URI invoked
 * @param message error message
 * @return number of bytes sent to the client
 */
size_t web::handleInternalError(WiFiClient *client, String *uri, const char *message) {
    //main status and headers
    size_t sz = client->println(http500Status);
    sz += client->println(hdJson);
    sz += writeDateHeader(client);
    sz += client->println(hdConClose);
    sz += client->println();

    //response body
    JsonDocument doc;
    doc["serverIP"] = WiFi.localIP();
    doc["uri"] = uri->c_str();
    doc["errorCode"] = 500;
    doc["errorMessage"] = message;
    //send it out
    sz += transmitJsonDocument(doc, client);
    sz += client->println();

#ifndef DISABLE_LOGGING
    Log.errorln(F("ERROR Handler handleInternalError for %s invoked: message %s"), uri->c_str(), message);
#endif
    return sz;
}

/**
 * Handler of resource not found errors (HTTP 404) - responds with JSON message
 * @param client the web client to respond to
 * @param uri URI invoked
 * @param message error message - included in a small html page in the response body (along with calling URI)
 * @return number of bytes sent to the client
 */
size_t web::handleNotFoundError(WiFiClient *client, String *uri, const char *message) {
    //main status and headers
    size_t sz = client->println(http404Status);
    sz += client->println(hdJson);
    sz += writeDateHeader(client);
    sz += client->println(hdConClose);
    sz += client->println();

    //response body
    JsonDocument doc;
    doc["serverIP"] = WiFi.localIP();
    doc["uri"] = uri->c_str();
    doc["errorCode"] = 404;
    doc["errorMessage"] = message;
    //send it out
    sz += transmitJsonDocument(doc, client);
    sz += client->println();

#ifndef DISABLE_LOGGING
    Log.errorln(F("ERROR Handler handleNotFoundError for %s invoked: message %s"), uri->c_str(), message);
#endif
    return sz;
}

/**
 * Dispatches an incoming request to its respective handler
 * <p>Other libraries worth having a look - currently all are archived, some have trouble building</p>
 * <ul>
 *  <li>AsyncWebServer library https://github.com/khoih-prog/AsyncWebServer_Ethernet</li>
 *  <li>AsyncWebServer for Raspberry Pico https://github.com/khoih-prog/AsyncWebServer_rp2040w</li>
 *  <li>Regular web server https://github.com/khoih-prog/WiFiWebServer</li>
 * </ul>
 */
void web::dispatch() {
    WiFiClient client = server.available();
    if (client) {
        unsigned long start = millis();
        IPAddress clientIp = client.remoteIP();
        Log.infoln(F("Request: inbound from %p"), clientIp);
        size_t szResp = 0;
        while (client.connected()) {
            if (client.available()) {
                String reqUri = client.readStringUntil('\n');
                String req = client.readString();
                reqUri.trim();
                String reqHeader, reqBody;
                int bodyMarker = req.indexOf("\r\n\r\n");
                if (bodyMarker >= 0) {
                    reqHeader = req.substring(0, bodyMarker);
                    reqBody = req.substring(bodyMarker + 4);
                    reqHeader.trim();
                    reqBody.trim();
                } else {
                    reqHeader = req;
                    reqHeader.trim();
                }
                reqUri.remove(reqUri.indexOf(" HTTP/"));
#ifndef DISABLE_LOGGING
                Log.infoln(F("Request data:\r\nURI: %s\r\n=== Headers ===\r\n%s\r\n=== Body ===\r\n%s\r\n======"), reqUri.c_str(), reqHeader.c_str(), reqBody.c_str());
#endif
                bool foundHandler = false;
                for (const auto &e : webMappings) {
                    //use regex to determine the URI
                    const std::regex re(e.first, std::regex_constants::ECMAScript | std::regex_constants::icase);
                    if (std::regex_search(reqUri.c_str(), re)) {
                        //found a handler - invoke it and signal we found a handler
                        szResp = e.second(&client, &reqUri, &reqHeader, &reqBody);
                        foundHandler = true;
                        break;
                    }
                }
                //default error handler for unmapped requests
                if (!foundHandler)
                    szResp = handleNotFoundError(&client, &reqUri, msgRequestNotMapped);
            }
            //done handling
            break;
        }
        // close the connection:
        client.stop();
        unsigned long dur = millis() - start;
        Log.infoln(F("Request: completed %u bytes [%u ms]"), szResp, dur);
    }
}
