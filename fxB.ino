/**
 * Tetris flavor 2 - single dot moving one direction, random color, stack at the end, constant speed between segments of 8
 */

const char fx02_description[] = "Moving a 1 pixel segment with constant speed and stacking at the end; flickering; Halloween colors - Halloween Tetris";
const CRGBPalette16 fx02_colors = CRGBPalette16(CRGB::Red, CRGB::Purple, CRGB::Orange, CRGB::Yellow);
uint fx02_cx = 0;
uint fx02_lastCx = 0;
uint fx02_speed = 100;
uint fx02_szStack = 0;

const uint8_t fx02_szSegment = 1;
const uint8_t fx02_szStackSeg = 1;
const uint8_t fx02_brightness = 140;
const uint8_t fx02_dimmed = 20;

uint fx02_incStackSize(int delta) {
  fx02_szStack = capr(fx02_szStack + delta, 0, FRAME_SIZE);
  return fx02_szStack;
}

uint fx02_stackAdjust() {
  if (fx02_szStack < fx02_szSegment << 1) {
    fx02_incStackSize(fx02_szStackSeg);
    return fx02_szStack;
  }
  if (fx02_cx < 16) {
    shiftRight(frame, FRAME_SIZE, 1);
    fx02_incStackSize(-1);
  } else if (inr(fx02_cx, 16, 32) || fx02_cx == fx02_lastCx) {
    fx02_incStackSize(-fx02_szStackSeg);
  } else
    fx02_incStackSize(fx02_szStackSeg);
  return fx02_szStack;
}


void fxb01_setup() {
  FastLED.clear(true);

  //shuffle led indexes
  fill_solid(frame, FRAME_SIZE, bkg);
  fx02_cx = 0;
  fx02_lastCx = 0;
  fx02_szStack = 0;
  mode = Chase;
}

void fxb01_run() {
  if (mode == TurnOff) {
    if (turnOff()) {
      fill_solid(frame, FRAME_SIZE, bkg);
      fx02_cx = 0;
      fx02_lastCx = 0;
      fx02_szStack = 0;      
    }
    return;
  }

  fx02_cx = random8();
  fx02_speed = random16(25, 201);
  int upLimit = FRAME_SIZE - fx02_szStack;
  // Move a single led
  for (int led = 0; led < upLimit; led++) {
    // Turn our current led on, then show the leds
    setTrailColor(frame, led, ColorFromPalette(fx02_colors, fx02_cx, random8(64, 193), LINEARBLEND), fx02_brightness, fx02_dimmed);
    pushFrame(frame, 0, true);
    FastLED.setBrightness(random8(30, 225));
    //fadeToBlackBy(leds, NUM_PIXELS, random16(40, 141));
    // Show the leds (only one of which is set to desired color, from above)
    FastLED.show();

    // Wait a little bit
    delay(fx02_speed);

    // Turn our current led back to black for the next loop around
    if (led < (upLimit - 1)) {
      offTrailColor(leds, led);
    }
  }

  fx02_stackAdjust();
  mode = fx02_szStack == FRAME_SIZE ? TurnOff : Chase;

  //save the color
  fx02_lastCx = fx02_cx;
}

