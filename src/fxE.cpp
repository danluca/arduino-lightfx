/* Display Template for FastLED
 *
 * By: Andrew Tuline
 * 
 * Modified by: Andrew Tuline
 *
 * Date: July, 2015
 *
 * This is a simple non-blocking FastLED display sequence template.
 *
 * 
 */
#include "fxE.h"

//~ Global variables definition
int twinkrate = 100;                                     // The higher the value, the lower the number of twinkles.
bool randhue =   true;                                     // Do we want random colours all the time? 1 = yes.
CRGBPalette16 currentPalette;
TBlendType    currentBlending;
FxE1 fxE1;
FxE2 fxE2;

FxE1::FxE1() {
    fxRegistry.registerEffect(this);
}

void FxE1::setup() {
    FastLED.clear(true);
    FastLED.setBrightness(BRIGHTNESS);
    currentBlending = LINEARBLEND;
    twinkrate = 100;
    thisdelay =  10;
    thisfade =   8;
    thishue =  50;
    thissat = 255;
    thisbri = 255;
    randhue =   1;
}

void FxE1::loop() {
    ChangeMe();                                                 // Check the demo loop for changes to the variables.

    EVERY_N_MILLISECONDS(100) {
        uint8_t maxChanges = 24;
        nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);   // AWESOME palette blending capability.
    }

    EVERY_N_MILLISECONDS(thisdelay) {                           // FastLED based non-blocking delay to update/display the sequence.
        twinkle();
    }

    EVERY_N_SECONDS(5) {                                        // Change the target palette to a random one every 5 seconds.
        //static uint8_t baseC = random8();                         // You can use this as a baseline colour if you want similar hues in the next line.
        targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
    }

    FastLED.show();
}

const char *FxE1::description() {
    return "FXE1: twinkle";
}


void FxE1::twinkle() {

  if (random8() < twinkrate) leds[random16(NUM_PIXELS)] += ColorFromPalette(currentPalette, (randhue ? random8() : thishue), 255, currentBlending);
  fadeToBlackBy(leds, NUM_PIXELS, thisfade);
  
} // twinkle()



void FxE1::ChangeMe() {                                             // A time (rather than loop) based demo sequencer. This gives us full control over the length of each sequence.

  uint8_t secondHand = (millis() / 1000) % 10;                // IMPORTANT!!! Change '15' to a different value to change duration of the loop.
  static uint8_t lastSecond = 99;                             // Static variable, means it's only defined once. This is our 'debounce' variable.
  if (lastSecond != secondHand) {                             // Debounce to make sure we're not repeating an assignment.
    lastSecond = secondHand;
    switch(secondHand) {
      case 0: thisdelay = 10; randhue = 1; thissat=255; thisfade=8; twinkrate=150; break;  // You can change values here, one at a time , or altogether.
      case 5: thisdelay = 100; randhue = 0;  thishue=random8(); thisfade=2; twinkrate=20; break;
      case 10: break;
    }
  }

} // e01_ChangeMe()

//=====================================
FxE2::FxE2() {
    fxRegistry.registerEffect(this);
}

void FxE2::setup() {
    fxE1.setup();
    currentPalette = RainbowColors_p;
}

void FxE2::loop() {
    beatwave();

    EVERY_N_MILLISECONDS(100) {
        uint8_t maxChanges = 24;
        nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);   // AWESOME palette blending capability.
    }

    EVERY_N_SECONDS(5) {                                        // Change the target palette to a random one every 5 seconds.
        targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
    }

    FastLED.show();
}

const char *FxE2::description() {
    return "FXE2: beatwave";
}


void FxE2::beatwave() {
  
  uint8_t wave1 = beatsin8(9, 0, 255);                        // That's the same as beatsin8(9);
  uint8_t wave2 = beatsin8(8, 0, 255);
  uint8_t wave3 = beatsin8(7, 0, 255);
  uint8_t wave4 = beatsin8(6, 0, 255);

  for (int i=0; i<NUM_PIXELS; i++) {
    leds[i] = ColorFromPalette( currentPalette, i+wave1+wave2+wave3+wave4, 255, currentBlending); 
  }
  
} // beatwave()
