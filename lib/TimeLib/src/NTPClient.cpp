/**
 * The MIT License (MIT)
 * Copyright (c) 2015,2025 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "NTPClient.h"
#include "TimeFormat.h"
#include <LogProxy.h>

#include "../../Utils/src/stringutils.h"

#define TWENTY_TWENTY    1577836800L    //2020-01-01 00:00
#define TWENTY_SEVENTY   3155760000L    //2070-01-01 00:00 - if this code is still relevant in 2070, something is wrong...

NTPClient::NTPClient(UDP& udp) {
  _udp = &udp;
}

NTPClient::NTPClient(UDP& udp, const char* poolServerName) {
  _udp = &udp;
  _poolServerName = poolServerName;
}

NTPClient::NTPClient(UDP& udp, const IPAddress &poolServerIP) {
  _udp = &udp;
  _poolServerIP = poolServerIP;
  _poolServerName = nullptr;
}

void NTPClient::begin(UDP* udp) {
  begin(NTP_DEFAULT_LOCAL_PORT, udp);
}

void NTPClient::begin(const unsigned int port, UDP* udp) {
  if (_udpSetup)
    return;

  if (udp == nullptr) {
    log_error(F("NTPClient: UDP instance is null"));
    return;
  }

  _udp = udp;

  _port = port;

  _udp->begin(_port);

  _udpSetup = true;
}

bool NTPClient::update(time_t &epochTime, int &wait) {
  log_debug(F("Update time from NTP Server %s"), _poolServerName);

  // flush any existing packets
  while(_udp->parsePacket() != 0)
    _udp->flush();

  sendNTPPacket();

  // Wait till data is there or timeout...
  uint16_t timeout = 0;
  int cb = 0;
  do {
    delay(20);
    cb = _udp->parsePacket();
    if (timeout > 250) {
      log_error(F("NTP update failed - timed out (50000ms)"));
      return false; // timeout after 1000 ms
    }
    timeout++;
  } while (cb == 0);
  wait = 10 * (timeout + 1); //account for the number of delays encountered

  // for packet structure see https://en.wikipedia.org/wiki/Network_Time_Protocol#NTP_packet_header_format and https://datatracker.ietf.org/doc/html/rfc5905
  // check if we got a proper packet - at least 44 bytes long - and whether the length received includes extension fields (causes packet length to exceed NTP_PACKET_SIZE)
  if (cb < 44) {
    log_error(F("NTP update failed - invalid/insufficient packet length %d bytes - required at least 44 bytes"), cb);
    return false;
  }
  _udp->read(_packetBuffer, NTP_PACKET_SIZE);
  log_debug(F("NTP update received packet [%d bytes] - %s"), cb, StringUtils::asHexString(_packetBuffer, NTP_PACKET_SIZE));
  // check the status of Stratum - 0 means invalid, and we've received a kiss-of-death code
  if (_packetBuffer[1] == 0) {
    log_error(F("NTP update failed - kiss-of-death received: "), String(_packetBuffer+12, 4).c_str());
    return false;
  }

  const unsigned long highWord = word(_packetBuffer[40], _packetBuffer[41]);
  const unsigned long lowWord = word(_packetBuffer[42], _packetBuffer[43]);
  // combine the four bytes (two words) into a long integer this is NTP time (seconds since Jan 1 1900):
  const unsigned long secsSince1900 = highWord << 16 | lowWord;
  if (cb > NTP_PACKET_SIZE) {
    log_warn(F("NTP update succeeded but packet length exceeds NTP_PACKET_SIZE (%d bytes) - ignoring extra data"), cb);
    // flush any remainder packets
    while(_udp->parsePacket() != 0)
      _udp->flush();
  }

  const time_t utcTime = static_cast<time_t>(secsSince1900) - SEVENTY_YEARS;
  log_info(F("NTP update successful - epoch time read as %llu UTC seconds since 1/1/1970 (%s)"), utcTime, TimeFormat::asString(utcTime, false).c_str());
  //we have seen issues with (WiFiNINA) UDP client stability/thread-safety (most likely) or NTP data (least likely);
  //the time extracted was in the year 2100 (!!) - adding sanity checks
  const bool validTime = utcTime > TWENTY_TWENTY && utcTime < TWENTY_SEVENTY;
  if (!validTime)
    log_error(F("NTP update succeeded but time is INVALID - outside the range %s - %s."), TimeFormat::asString(TWENTY_TWENTY, false).c_str(),
      TimeFormat::asString(TWENTY_SEVENTY, false).c_str());
  else
    epochTime = utcTime;

  return validTime;
}

void NTPClient::end() {
  if (_udp != nullptr)
    _udp->stop();
  _udpSetup = false;
}

void NTPClient::setPoolServerName(const char* poolServerName) {
  _poolServerName = poolServerName;
}

void NTPClient::sendNTPPacket() {
  // set all bytes in the buffer to 0
  memset(_packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form an NTP request
  _packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  _packetBuffer[1] = 0;     // Stratum, or type of clock
  _packetBuffer[2] = 6;     // Polling Interval
  _packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  _packetBuffer[12]  = 49;
  _packetBuffer[13]  = 0x4E;
  _packetBuffer[14]  = 49;
  _packetBuffer[15]  = 52;

  // all NTP fields have been given values; now you can send a packet requesting a timestamp:
  if (_poolServerName) {
    _udp->beginPacket(_poolServerName, 123);
  } else {
    _udp->beginPacket(_poolServerIP, 123);
  }
  _udp->write(_packetBuffer, NTP_PACKET_SIZE);
  _udp->endPacket();
}

void NTPClient::setRandomPort(const unsigned int minValue, const unsigned int maxValue) {
  // randomSeed(analogRead(0));
  // random(static_cast<long>(minValue), static_cast<long>(maxValue));  //this seems to cause issues on NanoConnect RP2040 boards
  _port = minValue + rp2040.hwrand32() % (maxValue - minValue + 1);
  log_debug(F("NTPClient: random port %d"), _port);
}
