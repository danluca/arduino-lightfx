/*
  WiFiServer.h - Library for Arduino WiFi shield.
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

#ifndef wifiserver_h
#define wifiserver_h

extern "C" {
  #include "utility/wl_definitions.h"
}

#include "Server.h"

class WiFiClient;

class WiFiServer : public Server {
  uint8_t _sock;
  uint8_t _lastSock;
  uint16_t _port;
  void*     pcb{};

public:
  WiFiServer();
  virtual ~WiFiServer() = default;
  explicit WiFiServer(uint16_t);
  WiFiClient available(uint8_t* status = nullptr);
  [[nodiscard]] WiFiClient accept() const;
  void begin() override;
  void begin(uint16_t port);
  void end();
  void close();
  void stop();
  void setNoDelay(bool nodelay) {};
  size_t write(uint8_t) override;
  size_t write(const uint8_t *buf, size_t size) override;
  [[nodiscard]] uint8_t status() const;
  explicit operator bool() const;

  using Print::write;
  using ClientType = WiFiClient;
};

#endif
