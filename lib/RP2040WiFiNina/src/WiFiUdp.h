/*
  WiFiUdp.h - Library for Arduino WiFi shield.
  Copyright (c) 2018 Arduino SA. All rights reserved.
  Copyright (c) 2011-2014 Arduino LLC.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef wifiudp_h
#define wifiudp_h

#include <Udp.h>

#define UDP_TX_PACKET_MAX_SIZE 24

class WiFiUDP : public UDP {
private:
  uint8_t _sock;  // socket ID for Wiz5100
  uint16_t _port{}; // local port to listen on
  int _parsed;

public:
  WiFiUDP();  // Constructor
  virtual ~WiFiUDP() = default;
  uint8_t begin(uint16_t) override;	// initialize, start listening on specified port. Returns 1 if successful, 0 if there are no sockets available to use
  uint8_t beginMulticast(IPAddress, uint16_t) override;  // initialize, start listening on specified multicast IP address and port. Returns 1 if successful, 0 if there are no sockets available to use
  void stop() override;  // Finish with the UDP socket

  // Sending UDP packets
  
  // Start building up a packet to send to the remote host specific in ip and port
  // Returns 1 if successful, 0 if there was a problem with the supplied IP address or port
  int beginPacket(IPAddress ip, uint16_t port) override;
  // Start building up a packet to send to the remote host specific in host and port
  // Returns 1 if successful, 0 if there was a problem resolving the hostname or port
  int beginPacket(const char *host, uint16_t port) override;
  // Finish off this packet and send it
  // Returns 1 if the packet was sent successfully, 0 if there was an error
  int endPacket() override;
  // Write a single byte into the packet
  size_t write(uint8_t) override;
  // Write size bytes from buffer into the packet
  size_t write(const uint8_t *buffer, size_t size) override;
  
  using Print::write;

  // Start processing the next available incoming packet
  // Returns the size of the packet in bytes, or 0 if no packets are available
  int parsePacket() override;
  // Number of bytes remaining in the current packet
  int available() override;
  // Read a single byte from the current packet
  int read() override;
  // Read up to len bytes from the current packet and place them into buffer
  // Returns the number of bytes read, or 0 if none are available
  int read(unsigned char* buffer, size_t len) override;
  // Read up to len characters from the current packet and place them into buffer
  // Returns the number of characters read, or 0 if none are available
  int read(char* buffer, const size_t len) override { return read((unsigned char*)buffer, len); };
  // Return the next byte from the current packet without moving on to the next byte
  int peek() override;
  void flush() override;	// Finish reading the current packet

  // Return the IP address of the host who sent the current incoming packet
  IPAddress remoteIP() override;
  // Return the port of the host who sent the current incoming packet
  uint16_t remotePort() override;

  friend class WiFiDrv;
};

#endif
