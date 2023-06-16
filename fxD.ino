/**
 * Tetris flavor 4 - single dot moving one direction, random color, stack at the end, constant speed between segments of 8
 * Same as flavor 1, but with a segment of 1 pixel only
 */

const char fx04_description[] = "Moving a 1 pixel segment with constant speed and stacking at the end;";
CRGBPalette16 fx04_colors = PartyColors_p;
uint fx04_cx = 0;
uint fx04_lastCx = 0;
uint fx04_speed = 100;
uint fx04_szStack = 0;
const uint8_t fx04_brightness = 140;
const uint8_t fx04_dimmed = 20;
const uint8_t fx04_szSegment = 1;
const uint8_t fx04_szStackSeg = 1;

uint fx04_incStackSize(int delta) {
  fx04_szStack = capr(fx04_szStack + delta, 0, NUM_PIXELS);
  return fx04_szStack;
}

uint fx04_stackAdjust() {
  if (fx04_szStack < fx04_szSegment << 1) {
    fx04_incStackSize(fx04_szStackSeg);
    return fx04_szStack;
  }
  if (fx04_cx < 16) {
    shiftRight(leds, NUM_PIXELS, 1);
    fx04_incStackSize(-1);
  } else if (inr(fx04_cx, 16, 32) || fx04_cx == fx04_lastCx) {
    fx04_incStackSize(-fx04_szStackSeg);
  } else
    fx04_incStackSize(fx04_szStackSeg);
  return fx04_szStack;
}

void fxd01_setup() {
  FastLED.clear(true);

  fx04_colors = PartyColors_p;
  //shuffle led indexes
  fill_solid(leds, NUM_PIXELS, bkg);
  fx04_cx = fx04_lastCx = fx04_szStack = 0;
  mode = Chase;
}

void fxd01_run() {
  if (mode == TurnOff) {
    if (turnOff()) {
      fill_solid(leds, NUM_PIXELS, bkg);
      fx04_cx = 0;
      fx04_lastCx = 0;
      fx04_szStack = 0;
      mode = Chase;
    }
    return;
  }
  fx04_cx = random8();
  fx04_speed = random16(25, 201);
  int upLimit = NUM_PIXELS - fx04_szStack;
  // Move a single led
  for (int led = 0; led < upLimit; led++) {
    // Turn our current led on, then show the leds
    setTrailColor(leds, led, ColorFromPalette(fx04_colors, fx04_cx, random8(fx04_dimmed+24, fx04_brightness), LINEARBLEND), fx04_brightness, fx04_dimmed);
    if (led == (upLimit-1))
      leds[led-1]=leds[led];

    // Show the leds (only one of which is set to desired color, from above)
    FastLED.show();

    // Wait a little bit
    delay(fx04_speed);

    // Turn our current led back to black for the next loop around
    if (led < (upLimit - 1)) {
      offTrailColor(leds, led);
    }
  }

  fx04_stackAdjust();
  mode = fx04_szStack == FRAME_SIZE ? TurnOff : Chase;

  //save the color
  fx04_lastCx = fx04_cx;
}

//=====================================
/* Confetti
By: Mark Kriegsman
Modified By: Andrew Tuline
Date: July 2015

Confetti flashes colours within a limited hue. It's been modified from Mark's original to support a few variables. It's a simple, but great looking routine.
*/

// Define variables used by the sequences.
uint8_t   d02_thisfade = 8;                                        // How quickly does it fade? Lower = slower fade rate.
int       d02_thishue = 50;                                       // Starting hue.
uint8_t   d02_thisinc = 1;                                        // Incremental value for rotating hues
uint8_t   d02_thissat = 100;                                      // The saturation, where 255 = brilliant colours.
uint8_t   d02_thisbri = 255;                                      // Brightness of a sequence. Remember, max_bright is the overall limiter.
int       d02_huediff = 256;                                      // Range of random #'s to use for hue
uint8_t   d02_thisdelay = 5;                                        // We don't need much delay (if any)

void fxd02_setup() {
  d02_thisfade = 8;
  d02_thishue = 50;
  d02_thisinc = 1;
  d02_thissat = 100;
  d02_thisbri = 255;
  d02_huediff = 256;
  d02_thisdelay = 5;
}

void fxd02_run () {
  d02_ChangeMe();                                                 // Check the demo loop for changes to the variables.

  EVERY_N_MILLISECONDS(d02_thisdelay) {                           // FastLED based non-blocking delay to update/display the sequence.
    confetti();
  }
  FastLED.show();  
} // loop()



void confetti() {                                             // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, NUM_PIXELS, d02_thisfade);                    // Low values = slower fade.
  int pos = random16(NUM_PIXELS);                               // Pick an LED at random.
  leds[pos] += CHSV((d02_thishue + random16(d02_huediff))/4 , d02_thissat, d02_thisbri);  // I use 12 bits for hue so that the hue increment isn't too quick.
  d02_thishue = d02_thishue + d02_thisinc;                                // It increments here.
} // confetti()


void d02_ChangeMe() {                                             // A time (rather than loop) based demo sequencer. This gives us full control over the length of each sequence.
  uint8_t secondHand = (millis() / 1000) % 15;                // IMPORTANT!!! Change '15' to a different value to change duration of the loop.
  static uint8_t lastSecond = 99;                             // Static variable, means it's only defined once. This is our 'debounce' variable.
  if (lastSecond != secondHand) {                             // Debounce to make sure we're not repeating an assignment.
    lastSecond = secondHand;
    switch(secondHand) {
      case  0: d02_thisinc=1; d02_thishue=192; d02_thissat=255; d02_thisfade=2; d02_huediff=256; break;  // You can change values here, one at a time , or altogether.
      case  5: d02_thisinc=2; d02_thishue=128; d02_thisfade=8; d02_huediff=64; break;
      case 10: d02_thisinc=1; d02_thishue=random16(255); d02_thisfade=1; d02_huediff=16; break;      // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
      case 15: break;                                                                // Here's the matching 15 for the other one.
    }
  }
} // d02_ChangeMe()
