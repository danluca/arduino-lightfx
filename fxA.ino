/**
 * Tetris flavors - single dot/segment moving one direction, random color, stack at the end, configurable various speed between segments of 8
 * 
 */

const char fxa_description[] = "Moving a fixed size segment with variable speed and stacking at the end - Tetris simplified";

const uint MAX_DOT_SIZE = 16;
const uint8_t fxa_brightness = 175;
const uint8_t fxa_dimmed = 20;
const CRGB bkg = CRGB::Black;
const uint FRAME_SIZE = 19;
const int turnOffSeq[] = { 1, 1, 2, 3, 5, 7, 10 };
enum OpMode { TurnOff, Chase };

CRGBPalette16 fxa_colors = RainbowColors_p;
uint fxa_cx = 0;
uint fxa_lastCx = 0;
uint fxa_speed = 100;
uint fxa_szSegment = 3;
uint fxa_szStackSeg = fxa_szSegment >> 1;
uint fxa_szStack = 0;
bool fxa_constSpeed = true;
OpMode mode = Chase;
int fxa_shuffleIndex[NUM_PIXELS];
CRGB dot[MAX_DOT_SIZE];
CRGB frame[NUM_PIXELS];

///////////////////////////////////////
// Support Utilities
///////////////////////////////////////
CRGB* makeDot(CRGB color, uint szDot) {
  dot[0] = color;
  dot[0] %= fxa_dimmed;
  for (uint x = 1; x < szDot; x++) {
    dot[x] = color;
    dot[x] %= fxa_brightness;
  }
  return dot;
}

bool isInViewport(int ledIndex, int viewportSize = FRAME_SIZE) {
  int viewportLow = 0;
  int viewportHi = viewportSize;
  return (ledIndex >= viewportLow) && (ledIndex < viewportHi);
}

bool isVisible(int ledIndex) {
  return (ledIndex >= 0) && (ledIndex < FRAME_SIZE);
}

uint validateSegmentSize(uint segSize) {
  return max(min(segSize, MAX_DOT_SIZE), 2);
}

void moveSeg(const CRGB dot[], uint szDot, CRGB dest[], uint lastPos, uint newPos, uint viewport) {
  bool rightDir = newPos >= lastPos;
  int bkgSeg = min(szDot, abs(newPos - lastPos));
  for (int x = 0; x < bkgSeg; x++) {
    int lx = rightDir ? lastPos + x : newPos + szDot + x;
    if (!isVisible(lx))
      continue;
    if (isInViewport(lx, viewport))
      dest[lx] = bkg;
  }
  for (int x = 0; x < szDot; x++) {
    int lx = newPos + x;
    if (!isVisible(lx))
      continue;
    if (isInViewport(lx, viewport + fxa_szStackSeg))
      dest[lx] = dot[rightDir ? x : szDot - 1 - x];
  }
}

void stack(CRGB color, CRGB dest[], uint stackStart) {
  for (int x = 0; x < fxa_szStackSeg; x++) {
    dest[stackStart + x] = color;
  }
}

uint fxa_incStackSize(int delta, uint max) {
  fxa_szStack = capr(fxa_szStack + delta, 0, max);
  return fxa_szStack;
}

uint fxa_stackAdjust(CRGB array[], uint szArray) {
  if (fxa_szStack < fxa_szSegment << 1) {
    fxa_incStackSize(fxa_szStackSeg, szArray);
    return fxa_szStack;
  }
  if (fxa_cx < 16) {
    shiftRight(array, szArray, 1);
    fxa_incStackSize(-1, szArray);
  } else if (inr(fxa_cx, 16, 32) || fxa_cx == fxa_lastCx) {
    fxa_incStackSize(-fxa_szStackSeg, szArray);
  } else
    fxa_incStackSize(fxa_szStackSeg, szArray);
  return fxa_szStack;
}


void moldWindow() {
  CRGB top[FRAME_SIZE];
  CRGB right[FRAME_SIZE];
  cloneArray(frame, top, FRAME_SIZE);
  fill_solid(right, FRAME_SIZE, bkg);
  copyArray(frame, 5, right, 5, 14);
  reverseArray(right, FRAME_SIZE);
  pushFrame(frame, 17);
  pushFrame(top, 19, 17);
  pushFrame(right, 14, 36);
}

void reset() {
  fill_solid(frame, FRAME_SIZE, bkg);
  fxa_szStack = 0;
  mode = Chase;
}

bool turnOff() {
  static int led = 0;
  static int xOffNow = 0;
  static uint szOffNow = turnOffSeq[xOffNow];
  static bool setOff = false;
  static bool allOff = false;

  EVERY_N_MILLISECONDS(25) {
    int totalLum = 0;
    for (int x = 0; x < szOffNow; x++) {
      int xled = fxa_shuffleIndex[(led + x)%FastLED.size()];
      FastLED.leds()[xled].fadeToBlackBy(12);
      totalLum += FastLED.leds()[xled].getLuma();
    }
    FastLED.show();
    setOff = totalLum<4;
  }

  EVERY_N_MILLISECONDS(1200) {
    if (setOff) {
      led = (led + szOffNow) % FastLED.size();
      xOffNow = capu(xOffNow + 1, sizeof(turnOffSeq) / sizeof(int) - 1);
      szOffNow = turnOffSeq[xOffNow];
      setOff = false;
    }
    allOff = !isAnyLedOn(FastLED.leds(), FastLED.size(), CRGB::Black);
  }
  //if we're turned off all LEDs, reset the static variables for next time
  if (allOff) {
    led = 0;
    xOffNow = 0;
    szOffNow = turnOffSeq[xOffNow];
    setOff = false;
    allOff = false;
    return true;
  }

  return false;
}

///////////////////////////////////////
// Effect Definitions - setup and loop
///////////////////////////////////////
void fxa01_setup() {
  FastLED.clear(true);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(dot, MAX_DOT_SIZE, bkg);

  fxa_colors = RainbowColors_p;
  //shuffle led indexes
  shuffleIndexes(fxa_shuffleIndex, NUM_PIXELS);
  reset();
  fxa_cx = 0;
  fxa_lastCx = 0;
  fxa_speed = 100;
  fxa_szStackSeg = fxa_szSegment >> 1;
}


void fxa01_run() {
  if (mode == TurnOff) {
    if (turnOff()) {
      reset();
    }
    return;
  }
  fxa_cx = random8();
  fxa_speed = random16(25, 201);
  fxa_szSegment = validateSegmentSize(fxa_szSegment);
  fxa_szStackSeg = fxa_szSegment >> 1;
  int szViewport = FRAME_SIZE - fxa_szStack;
  int startIndex = 0 - fxa_szSegment;

  // Make a dot with current color
  // makeDot(fxa_colors[fxa_cx], fxa_szSegment);
  makeDot(ColorFromPalette(fxa_colors, fxa_cx, random8(fxa_dimmed+24, fxa_brightness), LINEARBLEND), fxa_szSegment);
  for (int led = startIndex; led < szViewport; led++) {
    //move the segment, build up stack
    moveSeg(dot, fxa_szSegment, frame, led, led + 1, szViewport);
    if (led == (szViewport - 1))
      stack(dot[fxa_szSegment - 1], frame, szViewport);

    // Show the updated leds
    //FastLED.show();
    //showFill(frame, FRAME_SIZE);
    moldWindow();
    FastLED.show();

    // Wait a little bit
    delay(fxa_speed);

    //update fxa_speed if in next segment of 10 leds
    if (!fxa_constSpeed && (led % 10 == 9) && random16(0, 2)) {
      fxa_speed = random16(25, 201);
    }
  }

  fxa_stackAdjust(frame, FRAME_SIZE);
  mode = fxa_szStack == FRAME_SIZE ? TurnOff : Chase;

  //save the color
  fxa_lastCx = fxa_cx;
}

//=====================================
void fxa02_setup() {
  FastLED.clear(true);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(dot, MAX_DOT_SIZE, CRGB::Black);

  //shuffle led indexes done by fxa01_setup
  fxa_colors = PartyColors_p;
  reset();
  fxa_cx = 0;
  fxa_lastCx = 0;
  fxa_speed = 100;
  fxa_szStackSeg = fxa_szSegment >> 1;
}

void fxa02_run() {
  if (mode == TurnOff) {
    if (turnOff()) {
      reset();
    }
    return;
  }

  fxa_cx = random8();
  fxa_speed = random16(25, 201);
  fxa_szSegment = random8(2, MAX_DOT_SIZE);
  int szViewport = FRAME_SIZE;
  int startIndex = 0 - fxa_szSegment;

  // Make a dot with current color
  makeDot(ColorFromPalette(fxa_colors, fxa_cx, random8(fxa_dimmed+24, fxa_brightness), LINEARBLEND), fxa_szSegment);
  //move dot to right
  for (int led = startIndex; led < szViewport; led++) {
    //move the segment, build up stack
    moveSeg(dot, fxa_szSegment, frame, led, led + 1, szViewport);
    //retain the last pixel for turning back
    if (led >= (szViewport - 2))
      frame[szViewport - 1] = dot[fxa_szSegment - 1];
    // Show the updated leds
    showFill(frame, FRAME_SIZE);
    // Wait a little bit
    delay(fxa_speed);
    //update fxa_speed if in next segment of 10 leds
    if (!fxa_constSpeed && (led % 10 == 9) && random16(0, 2)) {
      fxa_speed = random16(25, 201);
    }
  }
  //move dot back to left
  for (int led = szViewport; led > startIndex; led--) {
    //move the segment, build up stack
    moveSeg(dot, fxa_szSegment, frame, led, led - 1, szViewport);
    // Show the updated leds
    showFill(frame, FRAME_SIZE);
    // Wait a little bit
    delay(fxa_speed);
    //update fxa_speed if in next segment of 10 leds
    if (!fxa_constSpeed && (led % 10 == 9) && random16(0, 2)) {
      fxa_speed = random16(25, 201);
    }
  }

  //save the color
  fxa_lastCx = fxa_cx;
}

//=====================================
/**
 * Tetris flavor 2 - single dot moving one direction, random color, stack at the end, constant speed between segments of 8
 */

const char fxb_description[] = "Moving a 1 pixel segment with constant speed and stacking at the end; flickering; Halloween colors - Halloween Tetris";
const CRGBPalette16 fxb_colors = CRGBPalette16(CRGB::Red, CRGB::Purple, CRGB::Orange, CRGB::Black);

//=====================================
void fxa03_setup() {
  FastLED.clear(true);
  FastLED.setBrightness(BRIGHTNESS);
  //shuffle led indexes
  fill_solid(frame, FRAME_SIZE, bkg);
  fxa_speed = 100;
  fxa_cx = 0;
  fxa_lastCx = 0;
  fxa_szStack = 0;
  mode = Chase;
}

void fxa03_run() {
  if (mode == TurnOff) {
    if (turnOff()) {
      fill_solid(frame, FRAME_SIZE, bkg);
      fxa_cx = 0;
      fxa_lastCx = 0;
      fxa_szStack = 0;
      mode = Chase;
    }
    return;
  }

  EVERY_N_MILLIS(fxa_speed<<2) {
    FastLED.setBrightness(random8(30, 225));
  }

  fxa_cx = random8();
  fxa_speed = random16(25, 201);
  int upLimit = FRAME_SIZE - fxa_szStack;
  // Move a single led
  for (int led = 0; led < upLimit; led++) {
    // Turn our current led on, then show the leds
    setTrailColor(frame, led, ColorFromPalette(fxb_colors, fxa_cx, random8(fxa_dimmed+24, fxa_brightness), LINEARBLEND), fxa_brightness, fxa_dimmed);
    pushFrame(frame, FRAME_SIZE, 0, true);
    //fadeToBlackBy(leds, NUM_PIXELS, random16(40, 141));
    // Show the leds (only one of which is set to desired color, from above)
    FastLED.show();

    // Wait a little bit
    delay(fxa_speed);

    // Turn our current led back to black for the next loop around
    if (led < (upLimit - 1)) {
      offTrailColor(leds, led);
    }
  }

  fxa_stackAdjust(frame, FRAME_SIZE);
  mode = fxa_szStack == FRAME_SIZE ? TurnOff : Chase;

  //save the color
  fxa_lastCx = fxa_cx;
}

/**
 * Tetris flavor 4 - single dot moving one direction, random color, stack at the end, constant speed between segments of 8
 * Same as flavor 1, but with a segment of 1 pixel only
 */

const uint8_t fxd_brightness = 140;

void fxa04_setup() {
  FastLED.clear(true);
  FastLED.setBrightness(BRIGHTNESS);

  fxa_colors = PartyColors_p;
  //shuffle led indexes
  fill_solid(leds, NUM_PIXELS, bkg);
  fxa_cx = fxa_lastCx = fxa_szStack = 0;
  fxa_szSegment = 1;
  fxa_speed = 100;
  mode = Chase;
}

void fxa04_run() {
  if (mode == TurnOff) {
    if (turnOff()) {
      fill_solid(leds, NUM_PIXELS, bkg);
      fxa_cx = 0;
      fxa_lastCx = 0;
      fxa_szStack = 0;
      mode = Chase;
    }
    return;
  }
  fxa_cx = random8();
  fxa_speed = random16(25, 201);
  int upLimit = NUM_PIXELS - fxa_szStack;
  // Move a single led
  for (int led = 0; led < upLimit; led++) {
    // Turn our current led on, then show the leds
    setTrailColor(leds, led, ColorFromPalette(fxa_colors, fxa_cx, random8(fxa_dimmed+24, fxd_brightness), LINEARBLEND), fxd_brightness, fxa_dimmed);
    if (led == (upLimit-1))
      leds[led-1]=leds[led];

    // Show the leds (only one of which is set to desired color, from above)
    FastLED.show();

    // Wait a little bit
    delay(fxa_speed);

    // Turn our current led back to black for the next loop around
    if (led < (upLimit - 1)) {
      offTrailColor(leds, led);
    }
  }

  fxa_stackAdjust(leds, NUM_PIXELS);
  mode = fxa_szStack == NUM_PIXELS ? TurnOff : Chase;

  //save the color
  fxa_lastCx = fxa_cx;
}



