/**
 * Tetris flavor 2 - single dot moving one direction, random color, stack at the end, constant speed between segments of 8
 */

const char fx02_description[] = "Moving a 1 pixel segment with constant speed and stacking at the end; flickering; Halloween colors - Halloween Tetris";
// CRGB fx02_colors[] = {CRGB::Red, CRGB::Purple, CRGB::OrangeRed, CRGB(0x66, 0x1B, 0x00), CRGB::Fuchsia, CRGB(0x10, 0x20, 0x04), CRGB::DarkRed, CRGB::OrangeRed};
const CRGBPalette16 fx02_colors = LavaColors_p;
uint fx02_cx = 0;
uint fx02_lastCx = 0;
uint fx02_speed = 100;
uint fx02_frzBound = 0;
const uint8_t fx02_brightness = 140;
const uint8_t fx02_dimmed = 20;
int fx02_shuffleIndex[NUM_PIXELS];
// const uint FX02_NUM_COLORS = sizeof(fx02_colors)/sizeof(CRGB);
const uint FX02_NUM_COLORS = 16;
const uint FX02_STACK_PUSH_COLOR = 5;

void fxb01_setup() {
  fill_solid(leds, NUM_PIXELS, CRGB::Black);

  //shuffle led indexes
  shuffleIndexes(fx02_shuffleIndex, NUM_PIXELS);
  fx02_frzBound = 0;
  clearSmooth();
}

void fxb01_run() {
  fx02_cx = random8();
  fx02_speed = random16(25, 201);
  int upLimit = NUM_PIXELS - fx02_frzBound;
  // Move a single led
  for (int led = 0; led < upLimit; led++) {
    // Turn our current led on, then show the leds
    setTrailColor(led, ColorFromPalette(fx02_colors, fx02_cx, random8(64, 193), LINEARBLEND), fx02_brightness, fx02_dimmed);

    FastLED.setBrightness(random8(30, 225));
    //fadeToBlackBy(leds, NUM_PIXELS, random16(40, 141));
    // Show the leds (only one of which is set to desired color, from above)
    FastLED.show();

    // Wait a little bit
    delay(fx02_speed);

    // Turn our current led back to black for the next loop around
    if (led < (upLimit - 1)) {
      offTrailColor(led);
    }
  }

  if ((fx02_frzBound>3) && (fx02_cx==FX02_STACK_PUSH_COLOR || fx02_cx==fx02_lastCx)) {
    if (fx02_cx == FX02_STACK_PUSH_COLOR) {
      shiftRight(leds, NUM_PIXELS, 1);
      random16_add_entropy(millis() >> 4);
      FastLED.show();
      delay(fx02_speed << 2);
    } else if (fx02_cx == fx02_lastCx) {
      fx02_frzBound--;
    }
  } else {
    fx02_frzBound = (fx02_frzBound + 1) % NUM_PIXELS;
  }

  if (fx02_frzBound == 0) {
    clearSmooth();
  }

  //save the color
  fx02_lastCx = fx02_cx;
}

