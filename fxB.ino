typedef void (*SimplePatternList[])();  // List of patterns to cycle through.  Each is defined as a separate function below.

const SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, fxb_confetti, sinelon, juggle, bpm };

uint8_t gCurrentPatternNumber = 0;
uint8_t gHue = 0;
uint8_t max_bright = 128;
CRGBPalette16 palette = PartyColors_p;

void fxb01_setup() {
  FastLED.clear(true);
  FastLED.setBrightness(BRIGHTNESS);
  gHue = 0;
  gCurrentPatternNumber = 0;
}

void fxb01_run() {
  EVERY_N_MILLISECONDS(100) {
    gPatterns[gCurrentPatternNumber]();  // Call the current pattern function once, updating the 'leds' array
  }

  EVERY_N_MILLISECONDS(40) {  // slowly cycle the "base color" through the rainbow
    gHue++;
  }

  EVERY_N_MINUTES(2) {
    gCurrentPatternNumber = inc(gCurrentPatternNumber, 1, arrSize(gPatterns));
  }

  FastLED.show();
}

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
