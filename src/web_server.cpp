#include "web_server.h"
#include "log.h"

const char http200Status[] PROGMEM = "HTTP/1.1 200 OK";
const char http303Status[] PROGMEM = "HTTP/1.1 303 See Other";
const char http404Status[] PROGMEM = "HTTP/1.1 404 Not Found";
const char http500Status[] PROGMEM = "HTTP/1.1 500 Internal Server Error";

const char hdHtml[] PROGMEM = R"===(Content-type: text/html
Server: rp2040-luca/1.0.0)===";

const char hdCss[] PROGMEM = R"===(Content-type: text/css
Server: rp2040-luca/1.0.0
Cache-Control: public, max-age=2592000, immutable)===";

const char hdJavascript[] PROGMEM = R"===(Content-type: text/javascript
Server: rp2040-luca/1.0.0
Cache-Control: public, max-age=2592000, immutable)===";

const char hdJson[] PROGMEM = R"===(Content-type: application/json
Server: rp2040-luca/1.0.0
Cache-Control: no-cache, no-store)===";

using namespace web;

const char hdRootLocation[] PROGMEM = "Location: /";
const char hdConClose[] PROGMEM = "Connection: close";
const char hdFmtDate[] PROGMEM = "Date: \"%4d-%02d-%02d %02d:%02d:%02d CST\"";
const char hdFmtContentDisposition[] PROGMEM = "Content-Disposition: inline; filename=\"%s\"";
const char msgRequestNotMapped[] PROGMEM = "URI not mapped to a handler on this server";
const char configJsonFilename[] PROGMEM = "config.json";
const char wifiJsonFilename[] PROGMEM = "wifi.json";
const char statusJsonFilename[] PROGMEM = "status.json";

/**
 * Web handler mappings - static in nature and stored in flash
 * The mapping consists of a string key - <code>Http_Method URI</code> associated with the function pointer that
 * handles the respective requests.
 * If more request mappings are needed - add them here, following the pattern
 */
const std::map<std::string, reqHandler> webMappings PROGMEM = {
        {"^GET /config\\.json$",  handleGetConfig},
        {"^GET /status\\.json$",  handleGetStatus},
        {"^GET /wifi\\.json$",    handleGetWifi},
        {"^GET /\\w+\\.css$",     handleGetCss},
        {"^GET /[\\w.]+\\.js$",      handleGetJs},
        {"^GET /\\w+\\.html$",    handleGetHtml},
        {"^GET /$",               handleGetRoot},
        {"^PUT /fx$",             handlePutConfig}
};

// global server object - through WiFi module
WiFiServer server(80);

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
    sprintf(buf, hdFmtContentDisposition, wifiJsonFilename);
    return client->println(buf);
}

/**
 * Handles <code>GET /wifi.json</code> - responds with JSON document containing WiFi connectivity details
 * <p>Must comply with the <code>reqHandler</code> function pointer signature</p>
 * @param client the web client to respond to
 * @param uri URI invoked
 * @param hd request headers
 * @param bdy request body - empty (this is a GET request)
 * @return number of bytes sent to the client
 */
size_t web::handleGetWifi(WiFiClient *client, String *uri, String *hd, String *bdy) {
    //main status and headers
    size_t sz = client->println(http200Status);
    sz += client->println(hdJson);
    sz += client->println(hdConClose);
    sz += writeDateHeader(client);
    sz += writeFilenameHeader(client, wifiJsonFilename);
    sz += client->println();    //done with headers

    // response body
    StaticJsonDocument<512> doc;

    //MAC address
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);
    char chrBuf[20];
    sprintf(chrBuf, "%X:%X:%X:%X:%X:%X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    doc["MAC"] = chrBuf;
    doc["IP"] = WiFi.localIP();         //IP Address
    const int32_t rssi = WiFi.RSSI();
    doc["RSSI"] = String(rssi);         //Wi-Fi signal level
    doc["bars"] = barSignalLevel(rssi);
    doc["millis"] = millis();           //current time in ms
    //current temperature
    if (IMU.temperatureAvailable()) {
        int temperature_deg = 0;
        IMU.readTemperature(temperature_deg);
        doc["boardTemp"] = temperature_deg;
    }
    doc["ntpSync"] = timeStatus();

    //send it out
    sz += serializeJson(doc, *client);
    sz += client->println();

#ifndef DISABLE_LOGGING
    Log.infoln(F("Handler handleGetWifi invoked for %s"), uri->c_str());
#endif
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
    StaticJsonDocument<4098> doc;

    doc["curEffect"] = String(fxRegistry.curEffectPos());
    doc["auto"] = fxRegistry.isAutoRoll();
    doc["curEffectName"] = fxRegistry.getCurrentEffect()->name();
    doc["holiday"] = holidayToString(paletteFactory.currentHoliday());
    JsonArray hldList = doc.createNestedArray("holidayList");
    for (uint8_t hi = None; hi <= NewYear; hi++)
        hldList.add(holidayToString(static_cast<Holiday>(hi)));
    time_t curTime = now();
    char datetime[20];
    sprintf(datetime, "%4d-%02d-%02d %02d:%02d:%02d", year(curTime), month(curTime), day(curTime), hour(curTime), minute(curTime), second(curTime));
    doc["currentTime"] = datetime;
    JsonArray fxArray = doc.createNestedArray("fx");
    fxRegistry.describeConfig(fxArray);
    //send it out
    sz += serializeJson(doc, *client);
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
    sz += client->println();    //done with headers

    // response body
    sz += client->println(pixel_css);
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
    sz += client->println();    //done with headers

    // response body
    if (uri->endsWith("jquery-ui.min.js"))
        sz += client->println(jquery_ui_min_js);
    else if (uri->endsWith("jquery.min.js"))
        sz += client->println(jquery_min_js);
    else
        sz += client->println(pixel_js);
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
    sz += client->println();    //done with headers

    // response body
    sz += client->println(index_html);
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
    StaticJsonDocument<512> doc;
    // WiFi
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["IP"] = WiFi.localIP();         //IP Address
    wifi["bars"] = barSignalLevel(WiFi.RSSI());  //Wi-Fi signal level
    // Fx
    JsonObject fx = doc.createNestedObject("fx");
    fx["count"] = fxRegistry.size();
    fx["auto"] = fxRegistry.isAutoRoll();
    fx["holiday"] = holidayToString(paletteFactory.currentHoliday());   //could be forced to a fixed value
    const LedEffect *curFx = fxRegistry.getCurrentEffect();
    fx["index"] = curFx->getRegistryIndex();
    fx["name"] = curFx->name();
    //fx["desc"] = curFx->description();    //variable size string - not really relevant, can be obtained from config
    // Time
    JsonObject time = doc.createNestedObject("time");
    time["ntpSync"] = timeStatus();
    time["millis"] = millis();           //current time in ms
    time_t curTime = now();
    char timeBuf[21];
    snprintf(timeBuf, 11, "%4d-%02d-%02d", year(curTime), month(curTime), day(curTime));    //counting the null terminator as well
    time["date"] = timeBuf;
    snprintf(timeBuf, 9, "%02d:%02d:%02d", hour(curTime), minute(curTime), second(curTime));
    time["time"] = timeBuf;
    time["holiday"] = holidayToString(currentHoliday());      //time derived holiday

    //current temperature
    if (IMU.temperatureAvailable()) {
        int temperature_deg = 0;
        IMU.readTemperature(temperature_deg);
        doc["boardTemp"] = temperature_deg;
    }

    //send it out
    sz += serializeJson(doc, *client);
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
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, *bdy);
    if (error)
        return handleInternalError(client, uri, error.c_str());

    bool autoAdvance = !doc.containsKey("auto") || doc["auto"].as<bool>();
    fxRegistry.autoRoll(autoAdvance);
    if (doc.containsKey("effect"))
        fxRegistry.nextEffectPos(abs(doc["effect"].as<int>()));
    if (doc.containsKey("holiday")) {
        String userHoliday = doc["holiday"].as<String>();
        paletteFactory.forceHoliday(parseHoliday(&userHoliday));
    }
#ifndef DISABLE_LOGGING
    Log.infoln(F("FX: Current running effect updated to %u, autoswitch %s, holiday %s"),
               fxRegistry.curEffectPos(), fxRegistry.isAutoRoll()?"true":"false", holidayToString(paletteFactory.currentHoliday()));
#endif

    //main status and headers
    size_t sz = client->println(http303Status);
    sz += client->println(hdRootLocation);
    sz += client->println();    //done with headers

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
    StaticJsonDocument<512> doc;
    doc["serverIP"] = WiFi.localIP();
    doc["uri"] = uri->c_str();
    doc["errorCode"] = 500;
    doc["errorMessage"] = message;
    //send it out
    sz += serializeJson(doc, *client);
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
    StaticJsonDocument<512> doc;
    doc["serverIP"] = WiFi.localIP();
    doc["uri"] = uri->c_str();
    doc["errorCode"] = 404;
    doc["errorMessage"] = message;
    //send it out
    sz += serializeJson(doc, *client);
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
