#include "net_setup.h"
#include "config.h"
#include "log.h"

const char ssid[] = WF_SSID;
const char pass[] = WF_PSW;

int status = WL_IDLE_STATUS;

WiFiServer server(80);
WiFiUDP Udp;  // A UDP instance to let us send and receive packets over UDP
NTPClient timeClient(Udp, CST_OFFSET_SECONDS);  //time client, retrieves time from pool.ntp.org for CST

bool wifi_setup() {
  bool result = false;
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
  result = status == WL_CONNECTED;

  // initialize the IMU (Inertial Measurement Unit)
  if (!IMU.begin()) {
    Log.errorln(F("Failed to initialize IMU!"));
    stateLED(CRGB::Red);
    result = false;
    //rtos::ThisThread::terminate();
    while (true) {
      yield();
    } 
  }

  //read the time
  bool ntpTimeAvailable = ntp_sync();
  setSyncProvider(curUnixTime);
  paletteFactory.adjustHoliday();
  result = result && ntpTimeAvailable;

  server.begin();                           // start the web server on port 80
  printWifiStatus();                        // you're connected now, so print out the status  
  return result;
}


void wifi_loop() {
  EVERY_N_MINUTES(15) {
    if (WiFi.status() != WL_CONNECTED) {
      Log.warningln(F("WiFi Connection lost - re-connecting..."));
      status = WL_CONNECTION_LOST;
      server.clearWriteError();
      wifi_setup();
    } else if (WiFi.ping(WiFi.gatewayIP()) < 0) {
        //ping test - can we ping the router? if not, disconnect and re-connect again, something must've gotten stale with the connection
        Log.warningln(F("Ping test failed, WiFi Connection unusable - re-connecting..."));
        WiFi.disconnect();
        status = WL_CONNECTION_LOST;
        server.clearWriteError();
        wifi_setup();
    }
    if (!timeClient.isTimeSet()) {
        bool ntpTimeAvailable = ntp_sync();
        Log.warningln(F("Acquiring NTP time, attempt %s"), ntpTimeAvailable ? "was successful" : "has FAILED, retrying later...");
        if (ntpTimeAvailable) {
            setSyncProvider(curUnixTime);
            paletteFactory.adjustHoliday();
        }
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
  long rssi = WiFi.RSSI();
  Log.infoln(F("Signal strength (RSSI): %d dBm"), rssi);

  // print the board temperature
  if (IMU.temperatureAvailable()) {
    int temperature_deg = 0;
    IMU.readTemperature(temperature_deg);
    Log.infoln(F("Board temperature %d °C"), temperature_deg);
  }

  if (timeStatus() != timeNotSet) {
      char timeBuf[20];
      sprintf(timeBuf, "%4d-%02d-%02d %02d:%02d:%02d", year(), month(), day(), hour(), minute(), second());
      Log.infoln(F("Current time %s"), timeBuf);
  } else
    Log.warningln(F("Current time not available - NTP sync failed"));

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
  return timeClient.isTimeSet() ? timeClient.getEpochTime() : WiFi.getTime();
}

bool ntp_sync() {
  timeClient.begin();
  timeClient.update();
  timeClient.end();
  return timeClient.isTimeSet();
}