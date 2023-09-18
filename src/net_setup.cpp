#include "net_setup.h"
#include "config.h"
#include "log.h"

const char ssid[] = WF_SSID;
const char pass[] = WF_PSW;

int status = WL_IDLE_STATUS;

WiFiUDP Udp;  // A UDP instance to let us send and receive packets over UDP
NTPClient timeClient(Udp, CST_OFFSET_SECONDS);  //time client, retrieves time from pool.ntp.org for CST
/**
 * Convenience to translate into number of bars the WiFi signal strength received from {@code WiFi.RSSI()}
 * <p>This Android article has been used for reference - https://android.stackexchange.com/questions/176320/rssi-range-for-wifi-icons </p>
 * @param rssi the RSSI value from WiFi system
 * @return number of bars as signal level - between 0 (no signal, connection likely lost) through 4 bars (strong signal)
 */
uint8_t barSignalLevel(int32_t rssi) {
    const static uint8_t numLevels = 5;
    const static int16_t minRSSI = -100;
    const static int16_t maxRSSI = -55;
    if (rssi <= minRSSI)
        return 0;
    else if (rssi >= maxRSSI)
        return numLevels - 1;
    float inRange = maxRSSI-minRSSI;
    float outRange = numLevels - 1;
    return (uint8_t )((float)(rssi - minRSSI) * outRange / inRange);
}

bool wifi_connect() {
    //static IP address - such that we can have a known location for config page
    WiFi.config({IP_ADDR}, {IP_DNS}, {IP_GW}, {IP_SUBNET});
    Log.infoln("Connecting to WiFI '%s'", ssid);  // print the network name (SSID);
    // attempt to connect to WiFi network:
    uint attCount = 0;
    while (status != WL_CONNECTED) {
        if (attCount > 60)
            stateLED(CRGB::Red);
        Log.infoln(F("Attempting to connect..."));

        // Connect to WPA/WPA2 network
        status = WiFi.begin(ssid, pass);
        // wait 10 seconds for connection to succeed:
        delay(10000);
        attCount++;
    }
    bool result = status == WL_CONNECTED;
    if (result) {
        int resPing = WiFi.ping(WiFi.gatewayIP());
        if (resPing >= 0)
            Log.infoln(F("Gateway ping successful: %d ms"), resPing);
        else
            Log.warningln(F("Failed pinging the gateway - will retry later"));
    }
    server_setup();     // start the web server on port 80
    printWifiStatus();  // you're connected now, so print out the status

    return result;
}

bool wifi_setup() {
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Log.warningln(F("Communication with WiFi module failed!"));
    // don't continue - terminate thread?
    //rtos::ThisThread::terminate();
    while (true) {
      yield();
    };
  }
  checkFirmwareVersion();
  
  //enable low power mode - web server is not the primary function of this module
  WiFi.lowPowerMode();

  return wifi_connect();
}

bool imu_setup() {
    // initialize the IMU (Inertial Measurement Unit)
    if (!IMU.begin()) {
        Log.errorln(F("Failed to initialize IMU!"));
        stateLED(CRGB::Red);
        //rtos::ThisThread::terminate();
        while (true) {
            yield();
        }
    }
    // print the board temperature
    bool result = IMU.temperatureAvailable();
    if (result) {
        int temperature_deg = 0;
        IMU.readTemperature(temperature_deg);
#ifndef DISABLE_LOGGING
        // F() macro doesn't seem to work well with UTF-8 chars, hence not using ° symbol for degree.
        // Can also try using wchar_t type. Unsure ArduinoLog library supports it well. All in all, not worth digging much into it - only used for troubleshooting
        Log.infoln(F("Board temperature %d 'C (%F 'F)"), temperature_deg, temperature_deg*9.0/5+32);
#endif
    }
    return result;
}

bool time_setup() {
    //read the time
    bool ntpTimeAvailable = ntp_sync();
    setSyncProvider(curUnixTime);
#ifndef DISABLE_LOGGING
    Log.warningln(F("Acquiring NTP time, attempt %s"), ntpTimeAvailable ? "was successful" : "has FAILED, retrying later...");
    Holiday hday = paletteFactory.adjustHoliday();
    if (timeStatus() != timeNotSet) {
        char timeBuf[20];
        sprintf(timeBuf, "%4d-%02d-%02d %02d:%02d:%02d", year(), month(), day(), hour(), minute(), second());
        Log.infoln(F("Current time %s CST"), timeBuf);
    } else
        Log.warningln(F("Current time not available - NTP sync failed"));
    Log.infoln(F("Current holiday is %s"), holidayToString(hday));
#endif
    return ntpTimeAvailable;
}

/**
 * WiFi connection check
 * @return true if all is ok, false if connection unusable
 */
bool wifi_check() {
    if (WiFi.status() != WL_CONNECTED) {
        Log.warningln(F("WiFi Connection lost"));
        return false;
    }
    int gwPingTime = WiFi.ping(WiFi.gatewayIP(), 64);
    int32_t rssi = WiFi.RSSI();
    uint8_t wifiBars = barSignalLevel(rssi);
    if ((gwPingTime < 0) || (wifiBars < 3)) {
        //we either cannot ping the router or the signal strength is 2 bars and under - reconnect for a better signal
        Log.warningln(F("Ping test failed (%d) or signal strength low (%d bars), WiFi Connection unusable"), rssi, wifiBars);
        return false;
    }
    Log.infoln(F("WiFi Ok - Gateway ping %d ms, RSSI %d (%d bars)"), gwPingTime, rssi, wifiBars);
    return true;
}

/**
 * Similar with wifi_connect, but with some preamble cleanup
 * Watch out: https://github.com/arduino/nina-fw/issues/63 - after WiFi reconnect, the server seems to stop working (returns disconnected clients?)
 * Should we invoke a board reset instead? (NVIC_SystemReset)
 */
void wifi_reconnect() {
    status = WL_CONNECTION_LOST;
    stateLED(CRGB::Orange);
    server.clearWriteError();
//    WiFiClient client = server.available();
//    if (client) client.stop();
//    Udp.stop();
//    WiFi.disconnect();
//    WiFi.end();
//    delay(2000);    //let disconnect state settle
    if (wifi_connect())
        stateLED(CRGB::Indigo);
    //NVIC_SystemReset();
}

void wifi_loop() {
  EVERY_N_MINUTES(7) {
    if (!wifi_check()) {
        Log.warningln(F("WiFi connection unusable/lost - reconnecting..."));
        wifi_reconnect();
    }

    if (!timeClient.isTimeSet()) {
        if (time_setup())
            stateLED(CRGB::Indigo);
        else
            stateLED(CRGB::Green);
    }
  }
  webserver();
  yield();
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Log.infoln(F("Connected to SSID: %s"), WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Log.infoln(F("IP Address: %p"), ip);

  // print your board's MAC address
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  Log.infoln(F("MAC Address %x:%x:%x:%x:%x:%x"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // print the received signal strength:
  int32_t rssi = WiFi.RSSI();
#ifndef DISABLE_LOGGING
  Log.infoln(F("Signal strength (RSSI) %d dBm; %d bars"), rssi, barSignalLevel(rssi));
#endif

  // print where to go in a browser:
  Log.infoln(F("To see this page in action, open a browser to http://%p"), ip);
}

void checkFirmwareVersion() {
  String fv = WiFi.firmwareVersion();
  Log.infoln(F("WiFi firmware version %s"), fv.c_str());
  //logInfo("WiFi current FW version: %s", fv);
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Log.warningln(F("Please upgrade the WiFi firmware to %s"), WIFI_FIRMWARE_LATEST_VERSION);
  }
}

///////////////////////////////////////
// Time sync
///////////////////////////////////////

time_t curUnixTime() {
  //Note - the WiFi.getTime() (returns unsigned long, 0 for failure) can also achieve this purpose
  //TODO: looks like we'd need to add the CST_OFFSET_SECONDS to the WiFi.getTime() as it returns the UTC
  return timeClient.isTimeSet() ? timeClient.getEpochTime() : WiFi.getTime();
}

bool ntp_sync() {
  timeClient.begin();
  timeClient.update();
  timeClient.end();
  return timeClient.isTimeSet();
}