//
// Copyright 2023,2024,2025 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_CONFIG_H
#define LIGHTFX_CONFIG_H

// the PWM pin dedicated for LED control - see the schematics as well as packages\framework-arduinopico\variants\pimoroni_plasma2350\pins_arduino.h
#define LED_PIN PIN_NEOPIXEL

#define MAX_NUM_PIXELS  1024    //maximum number of pixels supported (equivalent of 330ft LED strips). If more are needed, we'd need to revisit memory allocation and PWM timings

// initial global brightness 0-255
#define BRIGHTNESS 255

// These are lists and need to be commas instead of dots e.g., for IP address 192.168.0.1 use 192,168,0,1 instead
// #define IP_DNS 8,8,8,8               // Google DNS
// #define IP_DNS 75,75,75,75           // Xfinity DNS
#define IP_DNS 192,168,0,1              // Local DNS
#define IP_GW 192,168,0,1           // default gateway (router)
#define IP_SUBNET 255,255,255,0     // usual subnet mask
#define BROADCAST_CLIENTS     10, 11, 12, 72, 182        //this is a CSV of last byte of board IP addresses
#define MDNS_CACHING_TIMEOUT_MS (60*60*1000)          // 60 minutes for discovered boards to persist in cache

// in some networks, reaching the default NTP server pool may be challenging due to the simple UDP client we use and stricter control imposed on the network traffic
// in those cases the best option is to define a local NTP server as proxy
// NTP options - if using local NTP server specify its IP address here; the default NTP server pool is set at 'pool.ntp.org'
#ifdef LOCAL_NTP_SERVER
#define NTP_SERVER_IP 192,168,0,58      //nas02.local has an NTP service running
#endif

// MODE 0 = connect to wifi
// MODE 1 = Access point mode
// #define MODE 0

// Board 1 is the dev/xmas tree board, board 2 is the tree lighting controller
#ifndef BOARD_ID
#define BOARD_ID    2
#endif

// DEV Board specific configurations
#if BOARD_ID == 1

// LED chipset info - https://shop.pimoroni.com/products/10m-addressable-rgb-led-star-wire?variant=41375620530259
#define COLOR_ORDER BGR
#define CHIPSET     WS2812B

#define NUM_PIXELS  66       //the number of pixels of the 10m LED Star Wire used for the Xmas tree
#define FRAME_SIZE  22       //the Xmas tree has 6 strands from the 66 pixels strip, so 22 pixels per 2-strand
#define PIXEL_BUFFER_SPACE  (4*FRAME_SIZE)    //number of pixels to reserve for secondary buffer (used for effects data maneuvering)

// static IP - alternatively, the router can be configured to reserve IPs based on MAC
#define IP_ADDR 192,168,0,139    //Board 1 (dev)
#define V3_3    3.317f      //measured 3V3 pin voltage in V
#define MV3_3    3317       //measured 3V3 pin voltage in mV - technically 1000*V3_3 - expressed as int
// measured resistive Vcc voltage divisor for A0 pin, in ohms
#define VCC_DIV_R4  22000
#define VCC_DIV_R5  4700
#define DEVICE_NAME  "Xmas2350"

#endif

// FX01 Board specific configurations
#if BOARD_ID == 2

// LED chipset info - https://www.amazon.com/dp/B0923TN5GV?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_1&th=1
#define COLOR_ORDER RGB
#define CHIPSET     WS2811

#define NUM_PIXELS  1000      //number of pixels on the blue pine tree (900 measured + reserve)
#define FRAME_SIZE  75
#define PIXEL_BUFFER_SPACE  (4*FRAME_SIZE)    //number of pixels to reserve for secondary buffer (used for effects data maneuvering)

// static IP - alternatively, the router can be configured to reserve IPs based on MAC
#define IP_ADDR 192,168,0,182    //Board 2
// measured 3V3 pin voltage (in V and mV)
#define V3_3    3.317
#define MV3_3    3317
// measured resistive Vcc voltage divisor for A0 pin, in ohms
#define VCC_DIV_R4  22000
#define VCC_DIV_R5  4700
#define DEVICE_NAME  "FXPine"

#endif


// When set to 1 (via this header or a compiler/build flag), the device will ignore
// any web requests attempting to change the current effect or automatic effect mode.
// Default is 0 (feature disabled) so web requests are honored.
#ifndef IGNORE_WEB_EFFECT_CHANGES
#define IGNORE_WEB_EFFECT_CHANGES 0
#endif

#endif //LIGHTFX_CONFIG_H
