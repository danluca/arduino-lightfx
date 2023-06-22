/**
 * aanimations
 *
 * By: Can't recall where I found this. Maybe Stefan Petrick.
 * 
 * Modified by: Andrew Tuline
 *
 * Date: January, 2017
 *
 * This sketch demonstrates how to blend between two animations running at the same time.
 */
#include "fxC.h"

//~ Global variables definition
CRGB leds2[NUM_PIXELS];
CRGB leds3[NUM_PIXELS];


void fxc01_setup() {
  FastLED.clear(true);
  FastLED.setBrightness(BRIGHTNESS);
}

void fxc01_run() {
  animationA();                                               // render the first animation into leds2   
  animationB();                                               // render the second animation into leds3

  uint8_t ratio = beatsin8(2);                                // Alternate between 0 and 255 every minute
  
  for (int i = 0; i < NUM_PIXELS; i++) {                        // mix the 2 arrays together
    leds[i] = blend( leds2[i], leds3[i], ratio );
  }

  FastLED.show();

}

void animationA() {                                             // running red stripe.

  for (int i = 0; i < NUM_PIXELS; i++) {
    uint8_t red = (millis() / 10) + (i * 12);                    // speed, length
    if (red > 128) red = 0;
    leds2[i] = CRGB(red, 0, 0);
  }
} // animationA()



void animationB() {                                               // running green stripe in opposite direction.
  for (int i = 0; i < NUM_PIXELS; i++) {
    uint8_t green = (millis() / 5) - (i * 12);                    // speed, length
    if (green > 128) green = 0;
    leds3[i] = CRGB(0, green, 0);
  }
} // animationB()

//=====================================
/* blur
 *
 * By: ????
 * 
 * Modified by: Andrew Tuline
 *
 * Date: July, 2015
 *
 * Let's try the blur function. If you look carefully at the animation, sometimes there's a smooth gradient between each LED.
 * Other times, the difference between them is quite significant. Thanks for the blur.
 *
 */

void fxc02_setup() {
  FastLED.clear(true);
  FastLED.setBrightness(BRIGHTNESS);
}

void fxc02_run() {

  uint8_t blurAmount = dim8_raw( beatsin8(3,64, 192) );       // A sinewave at 3 Hz with values ranging from 64 to 192.
  blur1d( leds, NUM_PIXELS, blurAmount);                        // Apply some blurring to whatever's already on the strip, which will eventually go black.
  
  uint8_t  i = beatsin8(  9, 0, NUM_PIXELS);
  uint8_t  j = beatsin8( 7, 0, NUM_PIXELS);
  uint8_t  k = beatsin8(  5, 0, NUM_PIXELS);
  
  // The color of each point shifts over time, each at a different speed.
  uint16_t ms = millis();  
  leds[(i+j)/2] = CHSV( ms / 29, 200, 255);
  leds[(j+k)/2] = CHSV( ms / 41, 200, 255);
  leds[(k+i)/2] = CHSV( ms / 73, 200, 255);
  leds[(k+i+j)/3] = CHSV( ms / 53, 200, 255);
  
  FastLED.show();
  
} // fxc02_run()
 