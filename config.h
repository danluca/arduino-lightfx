#pragma once
// the PWM pin dedicated for LED control - see PinNames for D2
#define LED_PIN 25

// LED chipset info
#define COLOR_ORDER BRG
#define CHIPSET     WS2811

// initial global brightness 0-255
#define BRIGHTNESS 255

// These are lists and need to be commas instead of dots eg. for IP address 192.168.0.1 use 192,168,0,1 instead
#define IP_ADDR 192,168,0,10
#define IP_DNS 8,8,8,8          // Google DNS
#define IP_GW 192,168,0,1       // default gateway (router)
#define IP_SUBNET 255,255,255,0 

// MODE 0 = connect to wifi
// MODE 1 = Access point mode
// #define MODE 0

