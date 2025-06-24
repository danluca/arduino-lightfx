#pragma once

#ifndef NTPCLIENT_H
#define NTPCLIENT_H

#include "Arduino.h"
#include <Udp.h>

#define SEVENTY_YEARS 2208988800UL
#define NTP_PACKET_SIZE 48
#define NTP_DEFAULT_LOCAL_PORT 1337

class NTPClient {
    UDP*          _udp;
    bool          _udpSetup       = false;

    const char*   _poolServerName = "pool.ntp.org"; // Default time server
    IPAddress     _poolServerIP;
    unsigned int  _port           = NTP_DEFAULT_LOCAL_PORT;

    byte          _packetBuffer[NTP_PACKET_SIZE]{};
    void          sendNTPPacket();

  public:
    explicit NTPClient(UDP& udp);
    NTPClient(UDP& udp, const char* poolServerName);
    NTPClient(UDP& udp, const IPAddress &poolServerIP);

    /**
     * Set time-server name
     *
     * @param poolServerName
     */
    void setPoolServerName(const char* poolServerName);

     /**
      * Set a random local port
      */
    void setRandomPort(unsigned int minValue = 49152, unsigned int maxValue = 65535);

    /**
     * Starts the underlying UDP client with the default local port
     */
    void begin();

    /**
     * Starts the underlying UDP client with the specified local port
     */
    void begin(unsigned int port);

    /**
     * This will force the update from the NTP Server.
     *
     * @return true on success, false on failure
     */
    bool update(time_t &epochTime, int &wait);

    /**
     * Stops the underlying UDP client
     */
    void end();
};

#endif