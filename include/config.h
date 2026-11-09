//
// Copyright by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_CONFIG_H
#define LIGHTFX_CONFIG_H

// the PWM pin dedicated for LED control - see the schematics as well as packages\framework-arduinopico\variants\pimoroni_plasma2350\pins_arduino.h
#define LED_PIN PIN_NEOPIXEL

// LED chipset info
#define COLOR_ORDER BGR
#define CHIPSET     WS2812B

#include "board_config.h"

// Active geometry is selected at boot from the bundled UID configuration table.
#define NUM_PIXELS  (activeBoardConfiguration().numPixels)
#define FRAME_SIZE  (activeBoardConfiguration().frameSize)

// initial global brightness 0-255
#define BRIGHTNESS 255

// These are lists and need to be commas instead of dots e.g., for IP address 192.168.0.1 use 192,168,0,1 instead
// #define IP_DNS 8,8,8,8               // Google DNS
// #define IP_DNS 75,75,75,75           // Xfinity DNS
#define IP_DNS 192,168,0,1              // Local DNS
#define IP_GW 192,168,0,1           // default gateway (router)
#define IP_SUBNET 255,255,255,0     // usual subnet mask
#define STATIC_BROADCAST_CLIENTS     10, 11, 12              //this is a CSV of *last byte* of the boards having static IP addresses
#define MDNS_CACHING_TIMEOUT_MS (60*60*1000)          // 60 minutes for discovered boards to persist in cache
#ifndef MESH_CONTROL_TIMEOUT_MINUTES
#define MESH_CONTROL_TIMEOUT_MINUTES 20              // Restore local FX state after 20 minutes without a master FX update
#endif

// in some networks, reaching the default NTP server pool may be challenging due to the simple UDP client we use and stricter control imposed on the network traffic
// in those cases the best option is to define a local NTP server as proxy
// NTP options - if using local NTP server specify its IP address here; the default NTP server pool is set at 'pool.ntp.org'
#ifdef LOCAL_NTP_SERVER
#define NTP_SERVER_IP 192,168,0,58      //nas02.local has an NTP service running
#endif

// MODE 0 = connect to wifi
// MODE 1 = Access point mode
// #define MODE 0

// When set to 1 (via this header or a compiler/build flag), the device will ignore
// any web requests attempting to change the current effect or automatic effect mode.
// Default is 0 (feature disabled) so web requests are honored.
#ifndef IGNORE_WEB_EFFECT_CHANGES
#define IGNORE_WEB_EFFECT_CHANGES 0
#endif

#endif //LIGHTFX_CONFIG_H
