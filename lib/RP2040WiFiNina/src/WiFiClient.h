/*
  WiFiClient.cpp - Library for Arduino WiFi shield.
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

#ifndef wificlient_h
#define wificlient_h
#include "Arduino.h"	
#include "Print.h"
#include "Client.h"
#include "IPAddress.h"

#define WL_STREAM_BUFFER_SIZE 256

class WiFiClient : public Client {

public:
  virtual ~WiFiClient() = default;

  WiFiClient();
  explicit WiFiClient(uint8_t sock);

  [[nodiscard]] uint8_t status() const;
  void setConnectionTimeout(const uint16_t timeout) {_connTimeout = timeout;}

  int connect(IPAddress ip, uint16_t port) override;
  int connect(const char *host, uint16_t port) override;
  virtual int connectSSL(IPAddress ip, uint16_t port);
  virtual int connectSSL(const char *host, uint16_t port);
  virtual int connectBearSSL(IPAddress ip, uint16_t port);
  virtual int connectBearSSL(const char *host, uint16_t port);
  size_t write(uint8_t) override;
  size_t write(const uint8_t *buf, size_t size) override;
  size_t write(Stream &stream);
  virtual size_t retry(const uint8_t *buf, size_t size, bool write);
  int available() override;
  int read() override;
  int read(uint8_t *buf, size_t size) override;
  int peek() override;
  virtual void setRetry(bool retry);
  void flush() override;
  void stop() override;
  uint8_t connected() override;
  operator bool() override;

  virtual IPAddress remoteIP();
  virtual uint16_t remotePort();

  friend class WiFiServer;
  friend class WiFiDrv;

  using Print::write;

private:
  static uint16_t _srcport;
  uint8_t _sock;   //not used
  uint16_t  _socket;
  uint16_t  _connTimeout = 0;
  bool _retrySend;
};

#endif
