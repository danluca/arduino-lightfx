#include "net_setup.h"
#include "config.h"

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
    logWarn("Communication with WiFi module failed!");
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
  logInfo("Connecting to WiFI '%s'", ssid);  // print the network name (SSID);
  // attempt to connect to WiFi network:
  uint attCount = 0;
  while (status != WL_CONNECTED) {
    if (attCount > 60)
      stateLED(CRGB::Red);
    logInfo("Attempting to connect...");

    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection to succeed:
    delay(10000);
    attCount++;
  }
  result = status == WL_CONNECTED;

  // initialize the IMU (Inertial Measurement Unit)
  if (!IMU.begin()) {
    logError("Failed to initialize IMU!");
    stateLED(CRGB::Red);
    result = false;
    //rtos::ThisThread::terminate();
    while (true) {
      yield();
    } 
  }

  //read the time
  result = result && ntp_sync();
  if (result) {
    setSyncProvider(curUnixTime);
  }

  server.begin();                           // start the web server on port 80
  printWifiStatus();                        // you're connected now, so print out the status  
  return result;
}


void wifi_loop() {
  EVERY_N_MINUTES(15) {
    if (WiFi.status() != WL_CONNECTED) {
      logWarn("WiFi Connection lost - re-connecting...");
      status = WL_CONNECTION_LOST;
      server.clearWriteError();
      wifi_setup();
    }
  }
  webserver();
  yield();
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  logInfo("Connected to SSID: %s", WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  logInfo("IP Address: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

  // print your board's MAC address
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  logInfo("MAC Address %X:%X:%X:%X:%X:%X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  logInfo("Signal strength (RSSI): %d dBm", rssi);

  // print the board temperature
  if (IMU.temperatureAvailable()) {
    int temperature_deg = 0;
    IMU.readTemperature(temperature_deg);
    logInfo("Board temperature %d °C", temperature_deg);
  }

  if (timeStatus() != timeNotSet)
    logInfo("Current time %d-%02d-%02d %02d:%02d:%02d", year(), month(), day(), hour(), minute(), second());
  else 
    logWarn("Current time not available - NTP sync failed");

  // print where to go in a browser:
  logInfo("To see this page in action, open a browser to http://%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void checkFirmwareVersion() {
  String fv = WiFi.firmwareVersion();
  logInfo("WiFi firmware version %s", fv.c_str());
  //logInfo("WiFi current FW version: %s", fv);
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    logWarn("Please upgrade the WiFi firmware to %s", WIFI_FIRMWARE_LATEST_VERSION);
  }
}

///////////////////////////////////////
// Time sync
///////////////////////////////////////

time_t curUnixTime() {
  return timeClient.getEpochTime();
}

bool ntp_sync() {
  timeClient.begin();
  timeClient.update();
  timeClient.end();
  return timeClient.isTimeSet();
}