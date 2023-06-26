#ifndef LIGHTFX_CONFIG_H
#define LIGHTFX_CONFIG_H

// the PWM pin dedicated for LED control - see PinNames for D2
#define LED_PIN 25

// LED chipset info
#define COLOR_ORDER BRG
#define CHIPSET     WS2811

#define MAX_NUM_PIXELS  1024    //maximum number of pixels supported (equivalent of 330ft LED strips). If more are needed, we'd need to revisit memory allocation and PWM timings

//#define NUM_PIXELS  305    //number of pixels on the house edge strip
#define NUM_PIXELS  50    //number of pixels on a single 16.5ft (5m) strip

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

#endif //LIGHTFX_CONFIG_H
