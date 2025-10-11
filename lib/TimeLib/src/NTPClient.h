#pragma once

#ifndef NTPCLIENT_H
#define NTPCLIENT_H

#include "Arduino.h"
#include <Udp.h>

#define SEVENTY_YEARS 2208988800UL
#define NTP_PACKET_SIZE 48
#define NTP_DEFAULT_LOCAL_PORT 1337

inline constexpr auto defaultNTPServerPool PROGMEM = "pool.ntp.org";

class NTPClient {
    UDP*          _udp = nullptr;
    bool          _udpSetup = false;

    const char*   _poolServerName = defaultNTPServerPool;
    IPAddress     _poolServerIP{};
    unsigned int  _port = NTP_DEFAULT_LOCAL_PORT;

    byte          _packetBuffer[NTP_PACKET_SIZE]{};
    void          sendNTPPacket();

  public:
    NTPClient() = default;
    explicit NTPClient(UDP& udp);
    NTPClient(UDP& udp, const char* poolServerName);
    NTPClient(UDP& udp, const IPAddress &poolServerIP);

    /**
     * Set time-server name
     * Mutually exclusive with setPoolServerIP
     * @param poolServerName
     */
    void setPoolServerName(const char* poolServerName);

    /**
     * Set time-server IP address
     * Mutually exclusive with setPoolServerName
     * @param ntpServerAddress The IP address of the NTP server
     */
    void setPoolServerIP(const IPAddress& ntpServerAddress);

     /**
      * Set a random local port
      */
    void setRandomPort(unsigned int minValue = 49152, unsigned int maxValue = 65535);

    /**
     * Starts the underlying UDP client with the default local port
     */
    void begin(UDP* udp = nullptr);

    /**
     * Starts the underlying UDP client with the specified local port
     */
    void begin(unsigned int port, UDP* udp = nullptr);

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