/*
  WiFiStorage.cpp - Library for Arduino boards based on NINA WiFi module.
  Copyright (c) 2018 Arduino SA. All rights reserved.

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

#include "WiFiStorage.h"

WiFiStorageFile WiFiStorageClass::open(const char *filename) {
	WiFiStorageFile file(filename);
	file.size();
	return file;
}

WiFiStorageFile WiFiStorageClass::open(const String& filename) {
	return open(filename.c_str());
}