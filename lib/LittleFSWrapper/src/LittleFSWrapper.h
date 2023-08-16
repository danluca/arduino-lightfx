/****************************************************************************************************************************
  LittleFSWrapper.h - Filesystem wrapper for LittleFS on the Mbed RP2040 in the context of LightFX project
  For MBED RP2040-based boards such as Nano_RP2040_Connect, RASPBERRY_PI_PICO, ADAFRUIT_FEATHER_RP2040 and GENERIC_RP2040.
  Written by Khoi Hoang, adapted by Dan Luca
  Sourced from https://github.com/khoih-prog/LittleFS_Mbed_RP2040

  Copyright (C) 2021  Khoi Hoang
  Copyright (C) 2023  Dan Luca

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************************************************************/

#ifndef ARDUINO_LIGHTFX_LITTLEFSWRAPPER_H
#define ARDUINO_LIGHTFX_LITTLEFSWRAPPER_H

#if ( defined(ARDUINO_ARCH_RP2040) && defined(ARDUINO_ARCH_MBED) )
    #warning Use MBED RP2040 (such as NANO_RP2040_CONNECT, RASPBERRY_PI_PICO) and LittleFS
#else
    #error This code is intended to run on the MBED RASPBERRY_PI_PICO platform! Please check your Board setting.
#endif

#define LFSW_RP2040_VERSION           "LittleFSWrapper RP2040 v1.2.0"

#define LFSW_RP2040_VERSION_MAJOR      1
#define LFSW_RP2040_VERSION_MINOR      2
#define LFSW_RP2040_VERSION_PATCH      0

#define LFSW_RP2040_VERSION_INT        1002000

#include <Arduino.h>

#include "FlashIAPBlockDevice.h"
#include "LittleFileSystem.h"
#include "mbed.h"

#include <stdio.h>
#include <errno.h>
#include <functional>

#include "BlockDevice.h"

#include "ArduinoLog.h"


#if !defined(RP2040_FLASH_SIZE)
    //actual amount of flash on Nano RP2040 connect is 16MB per data sheet https://content.arduino.cc/assets/ABX00053-datasheet.pdf
    //the platform is configured to use 2MB of flash space - plenty for common needs. For the filesystem, we allocate space at the end of 4MB space.
    //if the platform is updated to a higher threshold of flash utilization, revisit this boundary below
    #define RP2040_FLASH_SIZE         (4 * 1024 * 1024)
#endif

#if !defined(RP2040_FS_LOCATION_END)
    #define RP2040_FS_LOCATION_END    RP2040_FLASH_SIZE
#endif

#if !defined(RP2040_FS_SIZE_KB)
    // Using default 64KB for LittleFS
    #define RP2040_FS_SIZE_KB       (64)
    #warning Using default RP2040_FS_SIZE_KB == 64KB
#else
    #warning Using RP2040_FS_SIZE_KB defined in external code
#endif

#if !defined(RP2040_FS_START)
    #define RP2040_FS_START           (RP2040_FLASH_SIZE - (RP2040_FS_SIZE_KB * 1024))
#endif

#define LITTLEFS_NAME           "lfs"
#define LITTLEFS_FILE_PREFIX    "/" LITTLEFS_NAME
#define LITTLEFS_ROOT_PATH      LITTLEFS_FILE_PREFIX "/"


static FlashIAPBlockDevice fsBD(XIP_BASE + RP2040_FS_START, (RP2040_FS_SIZE_KB * 1024));
static mbed::LittleFileSystem lfs(LITTLEFS_NAME);    //this can also be LittleFileSystemv2 for more features

class LittleFSWrapper {
public:
    LittleFSWrapper() {
        size = RP2040_FS_SIZE_KB * 1024;
        mounted = false;
    }
    ~LittleFSWrapper() {
        unmount();
    }

    bool init() {
#ifndef DISABLE_LOGGING
        Log.infoln(F("Initializing LittleFS with a size of %d KB"), RP2040_FS_SIZE_KB);
#endif
        if (!mounted) {
            int err = lfs.mount(&fsBD);
            if (err) {
                Log.errorln(F("LittleFS failed to mount: %d (%s). Reformatting..."), err, strerror(err));
                err = reformat() ? 0 : -5;
            } else
                Log.infoln(F("Successfully mounted LittleFS"));
            mounted = (err == 0);
        }

#ifndef DISABLE_LOGGING
        if (mounted) {
            struct statvfs stat;
            int err = statvfs(LITTLEFS_FILE_PREFIX "/", &stat);
            if (err == 0)
                Log.infoln(F("LittleFS stats - ID %u, capacity %u B, available %u B, available for unprivileged %u B"),
                           stat.f_fsid, stat.f_bsize*stat.f_blocks, stat.f_bsize*stat.f_bfree, stat.f_bsize*stat.f_bavail);
            else
                Log.errorln(F("Cannot gather LittleFS stats: %d (%s)"), err, strerror(err));
        }
#endif
        return mounted;
    }

    bool unmount() {
        if (mounted) {
            int err = lfs.unmount();
            if (err == 0)
                Log.infoln(F("Successfully unmounted LittleFS"));
            else {
                Log.errorln(F("Failed to unmount LittleFS: %d (%s)"), err, strerror(err));
                return false;
            }
        }

        mounted = false;
        return true;
    }

    static bool reformat() {
        Log.warningln(F("Reformatting LittleFS - all data will be lost!"));
        int err = lfs.reformat(&fsBD);
        if (err)
            Log.errorln(F("LittleFS failed to re-format: %d (%s)"), err, strerror(err));
        return err == 0;
    }

    static const char* getRoot() {
        return LITTLEFS_ROOT_PATH;
    }

private:
    uint32_t size;
    bool mounted;
};


#endif //ARDUINO_LIGHTFX_LITTLEFSWRAPPER_H
