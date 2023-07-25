#include "web_server.h"
#include "log.h"

const char http200Text[] PROGMEM = R"===(
HTTP/1.1 200 OK
Content-type: text/html
Server: rp2040-luca/1.0.0

)===";

const char http200Css[] PROGMEM = R"===(
HTTP/1.1 200 OK
Content-type: text/css
Server: rp2040-luca/1.0.0
Cache-Control: public, max-age=2592000, immutable

)===";

const char http200Javascript[] PROGMEM = R"===(
HTTP/1.1 200 OK
Content-type: text/javascript
Server: rp2040-luca/1.0.0
Cache-Control: public, max-age=2592000, immutable

)===";

const char http200Json[] PROGMEM = R"===(
HTTP/1.1 200 OK
Content-type: application/json
Server: rp2040-luca/1.0.0
Connection: close
Cache-Control: no-cache, no-store

)===";

const char http303Main[] PROGMEM = R"===(
HTTP/1.1 303 See Other
Location: /

)===";

const char http400Text[] PROGMEM = R"===(
HTTP/1.1 404 Not Found
Content-Type: text/html
Server: rp2040-luca/1.0.0
Connection: close

)===";

const char http500Text[] PROGMEM = R"===(
HTTP/1.1 500 Internal Server Error
Content-Type: text/html
Server: rp2040-luca/1.0.0
Connection: close

)===";

void sendContent(WiFiClient client, const char* name, const char* content) {
  // body/content of the HTTP response
  uint sz = client.print(content);
  // The HTTP response ends with another blank line:
  sz += client.println();
  //sz = strlen_P(content);
  Log.infoln(F("Response: HTTP 200 %s [%d bytes]"), name, sz);  //responses are long and static, not logging the body
}

void sendMainPage(WiFiClient client) {
  // response HTTP 1.1 preamble
  client.print(http200Text);
  // body/content of the HTTP response
  sendContent(client, "index.html", index_html);
}

void sendJavascript(WiFiClient client, const char* jsName, const char* jsContent) {
    // response HTTP 1.1 preamble
  client.print(http200Javascript);
  // body/content of the HTTP response
  sendContent(client, jsName, jsContent);
}

void sendCss(WiFiClient client, const char* cssName, const char* cssContent) {
    // response HTTP 1.1 preamble
  client.print(http200Css);
  // body/content of the HTTP response
  sendContent(client, cssName, cssContent);
}

void sendWifiJson(WiFiClient client) {
  //response header
  client.print(http200Json);
  // response body
  StaticJsonDocument<512> doc;

  //MAC address
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  char chrBuf[20];
  sprintf(chrBuf, "%X:%X:%X:%X:%X:%X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  doc["MAC"] = chrBuf;
  //IP Address
  IPAddress ip = WiFi.localIP();
  sprintf(chrBuf, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  doc["IP"] = chrBuf;
  //Wi-Fi signal level
  doc["RSSI"] = String(WiFi.RSSI());
  //current time
  doc["millis"] = millis();
  //current temperature
  if (IMU.temperatureAvailable()) {
    int temperature_deg = 0;
    IMU.readTemperature(temperature_deg);
    doc["temperature"] = temperature_deg;
  }
  doc["ntpSync"] = timeStatus();

  //send it out
  uint bodyLen = serializeJson(doc, client);
  bodyLen += client.println();
  Log.infoln(F("Response: HTTP 200 wifi.json [%u bytes]"), bodyLen);
}

void sendConfigJson(WiFiClient client) {
  //response header
  client.print(http200Json);
  // response body
  StaticJsonDocument<4098> doc;

  doc["curEffect"] = String(fxRegistry.curEffectPos());
  doc["auto"] = fxRegistry.isAutoRoll();
  doc["curEffectName"] = fxRegistry.getCurrentEffect()->name();
  doc["holiday"] = holidayToString(paletteFactory.currentHoliday());
  char datetime[20];
  sprintf(datetime, "%4d-%02d-%02d %02d:%02d:%02d", year(), month(), day(), hour(), minute(), second());
  doc["currentTime"] = datetime;
  JsonArray fxArray = doc.createNestedArray("fx");
  fxRegistry.describeConfig(fxArray);
  //send it out
  uint bodyLen = serializeJson(doc, client);
  bodyLen += client.println();
  Log.infoln(F("Response: HTTP 200 config.json [%u bytes]"), bodyLen);
}


void webserver() {
  WiFiClient client = server.available();
  if (client) {
    unsigned long start = millis();
    IPAddress clientIp = client.remoteIP();
    Log.infoln(F("Request: %p WEB start"), clientIp);
    //add some real entropy
    random16_add_entropy(start >> 4);
    while (client.connected()) {
      if (client.available()) {
        String reqUri = client.readStringUntil('\n');
        String reqHost = client.readStringUntil('\n');
        //client.findUntil("\r\n\r\n");
        String req = client.readString();
        int bodyMarker = req.indexOf("\r\n\r\n");
        String reqHeader = req.substring(0, bodyMarker);
        String reqBody = req.substring(bodyMarker+4);

        //trim the strings and log them
        reqUri.trim();
        reqHost.trim();
        req.trim();
        Log.infoln(reqUri.c_str());
        Log.infoln(reqHost.c_str());
        Log.infoln(req.c_str());

        //handle the request
        if (reqUri.startsWith("GET / ") || reqUri.startsWith("GET /index.html ")) {
          sendMainPage(client);
        } else if (reqUri.startsWith("GET /jquery.min.js ")) {
          sendJavascript(client, "jquery.min.js", jquery_min_js);
        } else if(reqUri.startsWith("GET /jquery-ui.min.js ")) {
          sendJavascript(client, "jquery-ui.min.js", jquery_ui_min_js);
        } else if (reqUri.startsWith("GET /pixel.css ")) {
          sendCss(client, "pixel.css", pixel_css);
        } else if (reqUri.startsWith("GET /pixel.js ")) {
          sendJavascript(client, "pixel.js", pixel_js);
        } else if (reqUri.startsWith("GET /config.json ")) {
          sendConfigJson(client);
        } else if (reqUri.startsWith("GET /wifi.json ")) {
          sendWifiJson(client);
        } else if (reqUri.startsWith("PUT /fx ")) {
          DynamicJsonDocument doc(512);
          // Parse JSON object
          DeserializationError error = deserializeJson(doc, reqBody);
          if (error) {
            client.print(http500Text);
            Log.infoln(F("Response: HTTP 500 [/fx]"));
          } else {
            bool autoAdvance = !doc.containsKey("auto") || doc["auto"].as<bool>();
            fxRegistry.autoRoll(autoAdvance);
            if (doc.containsKey("effect"))
                fxRegistry.nextEffectPos(abs(doc["effect"].as<int>()));
            if (doc.containsKey("holiday")) {
                String userHoliday = doc["holiday"].as<String>();
                paletteFactory.forceHoliday(parseHoliday(&userHoliday));
            }
            Log.infoln(F("FX: Current running effect updated to %u, autoswitch %s, holiday %s"),
                       fxRegistry.curEffectPos(), fxRegistry.isAutoRoll()?"true":"false", holidayToString(paletteFactory.currentHoliday()));
            client.print(http303Main);
            Log.infoln(F("Response: HTTP 303 [/fx]"));
          }
        } else {
          //unsupported - return 400
          client.print(http400Text);
          Log.errorln(F("Response: HTTP 404 NotFound"));
        }
        //done handling
        break;
      }
    }
    // close the connection:
    client.stop();
    unsigned long dur = millis() - start;
    Log.infoln(F("Request: WEB completed [%u ms]"), dur);
  }
}
