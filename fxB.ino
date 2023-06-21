//typedef void (*SimplePatternList[])();  // List of patterns to cycle through.  Each is defined as a separate function below.

uint8_t gHue = 0;
uint8_t max_bright = 128;
CRGBPalette16 palette = PartyColors_p;
CRGBPalette16 targetPalette;

void fxb01_setup() {
  FastLED.clear(true);
  FastLED.setBrightness(BRIGHTNESS);
  palette = PartyColors_p;
  gHue = 0;
  max_bright = 128;
}

void fxb01_run() {
  EVERY_N_MILLISECONDS(100) {
    rainbow();
    gHue++;
  }
  FastLED.show();
}

void fxb02_setup() {
  fxb01_setup();
}

void fxb02_run() {
  EVERY_N_MILLISECONDS(100) {
    rainbowWithGlitter();
    gHue++;
  }

  FastLED.show();
}

void fxb03_setup() {
  fxb01_setup();
}

void fxb03_run() {
  EVERY_N_MILLISECONDS(50) {
    fxb_confetti();
  }

  EVERY_N_MILLISECONDS(150) {  // slowly cycle the "base color" through the rainbow
    gHue++;
  }

  FastLED.show();
}

void fxb04_setup() {
  fxb01_setup();
}

void fxb04_run() {
  EVERY_N_MILLISECONDS(50) {
    sinelon();
  }

  EVERY_N_MILLISECONDS(100) {  // slowly cycle the "base color" through the rainbow
    gHue++;
  }

  FastLED.show();
}

void fxb05_setup() {
  fxb01_setup();
}

void fxb05_run() {
  EVERY_N_MILLISECONDS(50) {
    juggle();
  }

  EVERY_N_MILLISECONDS(100) {  // slowly cycle the "base color" through the rainbow
    gHue++;
  }

  FastLED.show();
}

void fxb06_setup() {
  fxb01_setup();
}

void fxb06_run() {
  EVERY_N_MILLISECONDS(100) {
    bpm();
  }

  EVERY_N_MILLISECONDS(200) {  // slowly cycle the "base color" through the rainbow
    gHue++;
  }

  FastLED.show();
}

void fxb07_setup() {
  fxb01_setup();
}

void fxb07_run() {
  EVERY_N_MILLISECONDS(50) {
    ease();
  }

  EVERY_N_MILLISECONDS(150) {  // slowly cycle the "base color" through the rainbow
    gHue++;
  }

  FastLED.show();
}

void fxb08_setup() {
  fxb01_setup();
}

void fxb08_run() {

  EVERY_N_MILLISECONDS(250) {                                                 // FastLED based non-blocking FIXED delay.
    uint8_t maxChanges = 24;
    nblendPaletteTowardPalette(palette, targetPalette, maxChanges);    // AWESOME palette blending capability.
  }

  EVERY_N_MILLISECONDS(50) {
    fadein();
  }

  EVERY_N_SECONDS(10) {                                                        // Change the target palette to a random one every 5 seconds.
    uint8_t baseC = random8(255);                                             // Use the built-in random number generator as we are re-initializing the FastLED one.
    targetPalette = CRGBPalette16(CHSV(baseC+random8(0,32), 255, random8(128, 255)), CHSV(baseC+random8(0,32), 255, random8(128, 255)), CHSV(baseC+random8(0,32), 192, random8(128, 255)), CHSV(baseC+random8(0,32), 255, random8(128, 255)));
  }

  FastLED.show();
  
} // loop()



void fadein() {

  random16_set_seed(535);                                                           // The randomizer needs to be re-set each time through the loop in order for the 'random' numbers to be the same each time through.

  for (int i = 0; i<NUM_PIXELS; i++) {
    uint8_t fader = sin8(millis()/random8(10,20));                                  // The random number for each 'i' will be the same every time.
    leds[i] = ColorFromPalette(palette, i*20, fader, LINEARBLEND);       // Now, let's run it through the palette lookup.
  }

  random16_set_seed(millis() >> 5);                                                      // Re-randomizing the random number seed for other routines.

} // fadein()


void rainbow() {

  fill_rainbow(leds, NUM_PIXELS, gHue, 7);  // FastLED's built-in rainbow generator.

}  // rainbow()



void rainbowWithGlitter() {

  rainbow();  // Built-in FastLED rainbow, plus some random sparkly glitter.
  nscale8(leds, NUM_PIXELS, fxa_brightness);
  addGlitter(80);

}  // rainbowWithGlitter()



void addGlitter(fract8 chanceOfGlitter) {

  if (random8() < chanceOfGlitter) {
    leds[random16(NUM_PIXELS)] += CRGB::White;
  }

}  // addGlitter()



void fxb_confetti() {  // Random colored speckles that blink in and fade smoothly.

  fadeToBlackBy(leds, NUM_PIXELS, 10);
  int pos = random16(NUM_PIXELS);
  leds[pos] += CHSV(gHue + random8(64), 200, 255);

}  // confetti()



void sinelon() {  // A colored dot sweeping back and forth, with fading trails.

  fadeToBlackBy(leds, NUM_PIXELS, 20);
  int pos = beatsin16(13, 0, NUM_PIXELS - 1);
  leds[pos] += CHSV(gHue, 255, 192);

}  // sinelon()



void bpm() {  // Colored stripes pulsing at a defined Beats-Per-Minute.

  uint8_t BeatsPerMinute = 62;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);

  for (int i = 0; i < NUM_PIXELS; i++) {  //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }

}  // bpm()



void juggle() {  // Eight colored dots, weaving in and out of sync with each other.

  fadeToBlackBy(leds, NUM_PIXELS, 20);
  byte dothue = 0;

  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, NUM_PIXELS - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }

}  // juggle()

void ease() {
  static uint8_t easeOutVal = 0;
  static uint8_t easeInVal  = 0;
  static uint8_t lerpVal    = 0;

  easeOutVal = ease8InOutQuad(easeInVal);                     // Start with easeInVal at 0 and then go to 255 for the full easing.
  easeInVal++;

  lerpVal = lerp8by8(0, NUM_PIXELS, easeOutVal);                // Map it to the number of LED's you have.

  leds[lerpVal] = ColorFromPalette(palette, gHue + (easeInVal << 1), 40 + easeOutVal);
  fadeToBlackBy(leds, NUM_PIXELS, 16);                          // 8 bit, 1 = slow fade, 255 = fast fade
 
} // ease()

