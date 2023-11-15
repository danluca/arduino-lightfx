//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_CONFIG_H
#define LIGHTFX_CONFIG_H

// the PWM pin dedicated for LED control - see PinNames for D2
#define LED_PIN 25

// LED chipset info
#define COLOR_ORDER BRG
#define CHIPSET     WS2811

#define MAX_NUM_PIXELS  1024    //maximum number of pixels supported (equivalent of 330ft LED strips). If more are needed, we'd need to revisit memory allocation and PWM timings

//#define NUM_PIXELS  300    //number of pixels on the house edge strip
#define NUM_PIXELS  320      //number of pixels on the house edge (300 measured + reserve)
#define FRAME_SIZE  68       //NOTE: frame size must be at least 3 times less than NUM_PIXELS. The frame CRGBArray must fit at least 3 frames

// initial global brightness 0-255
#define BRIGHTNESS 255

// These are lists and need to be commas instead of dots eg. for IP address 192.168.0.1 use 192,168,0,1 instead
#define IP_DNS 8,8,8,8          // Google DNS
#define IP_GW 192,168,0,1       // default gateway (router)
#define IP_SUBNET 255,255,255,0 

// MODE 0 = connect to wifi
// MODE 1 = Access point mode
// #define MODE 0

// Board 1 is the dev board. We have 2 boards.
#define BOARD_ID    1

#if BOARD_ID == 1

// static IP - alternatively, the router can be configured to reserve IPs based on MAC
#define IP_ADDR 192,168,0,10    //Board 1 (dev)
#define V3_3    3.262f      //measured 3V3 pin voltage in V
#define MV3_3    3262       //measured 3V3 pin voltage in mV - technically 1000*V3_3 - expressed as int
// measured Vcc voltage divisor for A0 pin
#define VCC_DIV_R4  21950
#define VCC_DIV_R5  3304
//TODO: need to measure the parameters below for this board:
// measured RP2040 internal temp sensor parameters - see RP2040 datasheet page 565
#define CHIP_RP2040_TEMP_SENSOR_VOLTAGE_27      604.499363f     //in mV
#define CHIP_RP2040_TEMP_SENSOR_VOLTAGE_SLOPE   1.807087f       //in mV/degree (value is negative, but that is accounted in the formula)
#define BOARD_NAME  "Dev"

#endif

#if BOARD_ID == 2

// static IP - alternatively, the router can be configured to reserve IPs based on MAC
#define IP_ADDR 192,168,0,11    //Board 2
// measured 3V3 pin voltage (in V and mV)
#define V3_3    3.242
#define MV3_3    3242
// measured Vcc voltage divisor for A0 pin
#define VCC_DIV_R4  21800
#define VCC_DIV_R5  3305
// measured RP2040 internal temp sensor parameters - see RP2040 datasheet page 565
#define CHIP_RP2040_TEMP_SENSOR_VOLTAGE_27      604.499363f     //in mV
#define CHIP_RP2040_TEMP_SENSOR_VOLTAGE_SLOPE   1.807087f       //in mV/degree (value is negative, but that is accounted in the formula)
#define BOARD_NAME  "FX01"

#endif


#endif //LIGHTFX_CONFIG_H
