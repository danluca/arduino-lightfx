///////////////////////////////////////////////////////////////////////////////////////////
// Fire2012 with programmable Color Palette standard example, broken into multiple segments
// Based on Fire2012 by Mark Kriegsman, July 2012 as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
//////////////////////////////////////////////////////////////////////////////////////////
#include "fxH.h"

FxH1 fxH1;
FxH2 fxH2;

// Four different static color palettes are provided here, plus one dynamic one.
//
// The three static ones are:
//   1. the FastLED built-in HeatColors_p -- this is the default, and it looks
//      pretty much exactly like the original Fire2012.
//
//  To use any of the other palettes below, just "uncomment" the corresponding code and make the gPal non-constant
//
//   2. a gradient from black to red to yellow to white, which is
//      visually similar to the HeatColors_p, and helps to illustrate
//      what the 'heat colors' palette is actually doing,
//   3. a similar gradient, but in blue colors rather than red ones,
//      i.e. from black to blue to aqua to white, which results in
//      an "icy blue" fire effect,
//   4. a simplified three-step gradient, from black to red to white, just to show
//      that these gradients need not have four components; two or
//      three are possible, too, even if they don't look quite as nice for fire.
//
// The dynamic palette shows how you can change the basic 'hue' of the
// color palette every time through the loop, producing "rainbow fire".

FxH1::FxH1() {
    fxRegistry.registerEffect(this);
}

void FxH1::setup() {
    FastLED.clear(true);
    FastLED.setBrightness(BRIGHTNESS);

    // This first palette is the basic 'black body radiation' colors, which run from black to red to bright yellow to white.
    //gPal = HeatColors_p;

    // These are other ways to set up the color palette for the 'fire'.
    // First, a gradient from black to red to yellow to white -- similar to HeatColors_p
    //   gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Yellow, CRGB::White);

    // Second, this palette is like the heat colors, but blue/aqua instead of red/yellow
    //   gPal = CRGBPalette16( CRGB::Black, CRGB::Blue, CRGB::Aqua,  CRGB::White);

    // Third, here's a simpler, three-step gradient, from black to red to white
    //   gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::White);
}

void FxH1::loop() {
    EVERY_N_MILLIS(1000 / FRAMES_PER_SECOND) {
        // Add entropy to random number generator; we use a lot of it.
        random16_add_entropy(random());

        // Fourth, the most sophisticated: this one sets up a new palette every
        // time through the loop, based on a hue that changes every time.
        // The palette is a gradient from black, to a dark color based on the hue,
        // to a light color based on the hue, to white.
        //
        //   static uint8_t hue = 0;
        //   hue++;
        //   CRGB darkcolor  = CHSV(hue,255,192); // pure hue, three-quarters brightness
        //   CRGB lightcolor = CHSV(hue,128,255); // half 'whitened', full brightness
        //   gPal = CRGBPalette16( CRGB::Black, darkcolor, lightcolor, CRGB::White);

        Fire2012WithPalette(fire1, FireEnd1-FireStart1, FireStart1, false);  // run simulation frame 1, using palette colors
        Fire2012WithPalette(fire2, FireEnd2-FireStart2, FireStart2, false);  // run simulation frame 2, using palette colors
        Fire2012WithPalette(fire3, FireEnd3-FireStart3, FireStart3, true);  // run simulation frame 3, using palette colors

        FastLED.show();  // display this frame
    }
}

const char *FxH1::description() {
    return "FXH1: Fire in 3 segments";
}


void FxH1::Fire2012WithPalette(int heat[], const uint szArray, const uint stripOffset, bool reverse) {
  if (szArray > MAX_ENGINE_SIZE)
    return;

  // Step 1.  Cool down every cell a little
  for (int i = 0; i < szArray; i++) {
    heat[i] = qsub8(heat[i], random8(0, ((COOLING * 10) / szArray) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for (int k = szArray - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if (random8() < SPARKING) {
    int y = random8(7);
    heat[y] = qadd8(heat[y], random8(160, 255));
  }

  // Step 4.  Map from heat cells to LED colors
  for (int j = 0; j < szArray; j++) {
    // Scale the heat value from 0-255 down to 0-240 for best results with color palettes.
    uint8_t colorindex = scale8(heat[j], 240);
    CRGB color = ColorFromPalette(gPal, colorindex);
    int pixelnumber = (reverse ? (szArray - 1 - j) : j) + stripOffset;
    leds[pixelnumber] = color;
    if (j > random8(5,9))
      leds[pixelnumber].nscale8(flameBrightness);
  }
}


///////////////////////////////////////////////////////////////////////////////////////////
// Confetti Pallette
//////////////////////////////////////////////////////////////////////////////////////////
FxH2::FxH2() {
    fxRegistry.registerEffect(this);
}

void FxH2::setup() {
    FastLED.clear(true);
    FastLED.setBrightness(BRIGHTNESS);

    thisfade = 8;
    thishue = 50;
    thisinc = 1;
    thissat = 100;
    thisbri = 255;
    huediff = 256;
    thisdelay = 5;
}

void FxH2::loop() {
    ChangeMe();  // Check the demo loop for changes to the variables.

    EVERY_N_MILLISECONDS(100) {
        uint8_t maxChanges = 24;
        nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);  // AWESOME palette blending capability.
    }

    EVERY_N_MILLISECONDS(thisdelay) {  // FastLED based non-blocking delay to update/display the sequence.
        confetti_pal();
    }

    FastLED.show();
}

const char *FxH2::description() {
    return "FXH2: confetti palette";
}

void FxH2::confetti_pal() {  // random colored speckles that blink in and fade smoothly

  fadeToBlackBy(leds, NUM_PIXELS, thisfade);  // Low values = slower fade.
  int pos = random16(NUM_PIXELS);             // Pick an LED at random.
  leds[pos] = ColorFromPalette(currentPalette, thishue + random16(huediff) / 4, thisbri, currentBlending);
  thishue = thishue + thisinc;  // It increments here.

}  // confetti_pal()



void FxH2::ChangeMe() {  // A time (rather than loop) based demo sequencer. This gives us full control over the length of each sequence.

  uint8_t secondHand = (millis() / 1000) % 15;  // IMPORTANT!!! Change '15' to a different value to change duration of the loop.
  static uint8_t lastSecond = 99;               // Static variable, means it's only defined once. This is our 'debounce' variable.
  if (lastSecond != secondHand) {               // Debounce to make sure we're not repeating an assignment.
    lastSecond = secondHand;
    switch (secondHand) {
      case 0:
        targetPalette = OceanColors_p;
        thisinc = 1;
        thishue = 192;
        thissat = 255;
        thisfade = 2;
        huediff = 255;
        break;  // You can change values here, one at a time , or altogether.
      case 5:
        targetPalette = LavaColors_p;
        thisinc = 2;
        thishue = 128;
        thisfade = 8;
        huediff = 64;
        break;
      case 10:
        targetPalette = ForestColors_p;
        thisinc = 1;
        thishue = random16(255);
        thisfade = 1;
        huediff = 16;
        break;         // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
      case 15: break;  // Here's the matching 15 for the other one.
    }
  }

}  // ChangeMe()
