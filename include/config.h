//
// Copyright 2023,2024 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_CONFIG_H
#define LIGHTFX_CONFIG_H

// the PWM pin dedicated for LED control - see PinNames for D2
#define LED_PIN 25

// LED chipset info
#define COLOR_ORDER BRG
#define CHIPSET     WS2811

#define MAX_NUM_PIXELS  1024    //maximum number of pixels supported (equivalent of 330ft LED strips). If more are needed, we'd need to revisit memory allocation and PWM timings

// initial global brightness 0-255
#define BRIGHTNESS 255

// These are lists and need to be commas instead of dots eg. for IP address 192.168.0.1 use 192,168,0,1 instead
#define IP_DNS 8,8,8,8          // Google DNS
#define IP_GW 192,168,0,1       // default gateway (router)
#define IP_SUBNET 255,255,255,0 

// MODE 0 = connect to wifi
// MODE 1 = Access point mode
// #define MODE 0

// Board 1 is the dev board, board 2 is the house lighting controller.
#define BOARD_ID    1

// Board specific configurations
#if BOARD_ID == 1

#define NUM_PIXELS  240      //number of pixels on the office window edge (need to be at least three times the frame size)
#define FRAME_SIZE  76       //NOTE: frame size must be at least 3 times less than NUM_PIXELS. The frame CRGBArray must fit at least 3 frames

// static IP - alternatively, the router can be configured to reserve IPs based on MAC
#define IP_ADDR 192,168,0,10    //Board 1 (dev)
#define V3_3    3.262f      //measured 3V3 pin voltage in V
#define MV3_3    3262       //measured 3V3 pin voltage in mV - technically 1000*V3_3 - expressed as int
// measured resistive Vcc voltage divisor for A0 pin, in ohms
#define VCC_DIV_R4  21950
#define VCC_DIV_R5  3304
#define DEVICE_NAME  "Dev"
#define BROADCAST_MASTER  true          //whether this board will push Fx changes to other boards
#define BROADCAST_CLIENTS     11        //this is a CSV of last byte of slave IP addresses

#endif

#if BOARD_ID == 2

#define NUM_PIXELS  320      //number of pixels on the house edge (300 measured + reserve)
#define FRAME_SIZE  68       //NOTE: frame size must be at least 3 times less than NUM_PIXELS. The frame CRGBArray must fit at least 3 frames

// static IP - alternatively, the router can be configured to reserve IPs based on MAC
#define IP_ADDR 192,168,0,11    //Board 2
// measured 3V3 pin voltage (in V and mV)
#define V3_3    3.242
#define MV3_3    3242
// measured resistive Vcc voltage divisor for A0 pin, in ohms
#define VCC_DIV_R4  21800
#define VCC_DIV_R5  3305
#define DEVICE_NAME  "FX01"
#define BROADCAST_MASTER  false           //whether this board will push Fx changes to other boards
#define BROADCAST_CLIENTS   0             //0 is for NA

#endif


#endif //LIGHTFX_CONFIG_H
