/**
 * Tetris flavor 1 - single dot/segment moving one direction, random color, stack at the end, configurable various speed between segments of 8
 * Couple of definitions: physical ledstrip has the first led at index 0. Virtual ledstrip has any length and can have indexes both <0 and >number of leds.
 * Viewport is the ledstrip between index 0 and the start of the stack. Stack is the portion of the ledstrip where the segments accumulate, there is no movement in the stack.
 */

const char fx01_description[] = "Moving a fixed size segment with variable speed and stacking at the end - Tetris simplified";
CRGBPalette16 fx01_colors = RainbowColors_p;
uint fx01_cx = 0;
uint fx01_lastCx = 0;
uint fx01_speed = 100;
const uint MAX_DOT_SIZE = 16;
const uint8_t fx01_brightness = 175;
const uint8_t fx01_dimmed = 20;
const CRGB bkg = CRGB::Black;
const uint FRAME_SIZE = 19;
const int turnOffSeq[] = { 1, 1, 2, 3, 5, 7, 10 };
enum OpMode { TurnOff, Chase };

uint fx01_szSegment = 3;
uint fx01_szStackSeg = fx01_szSegment >> 1;
uint fx01_szStack = 0;
CRGB dot[MAX_DOT_SIZE];
CRGB frame[FRAME_SIZE];
int fx01_shuffleIndex[NUM_PIXELS];
bool fx01_constSpeed = true;
OpMode mode = Chase;

CRGB* makeDot(CRGB color, uint szDot) {
  dot[0] = color;
  dot[0] %= fx01_dimmed;
  for (uint x = 1; x < szDot; x++) {
    dot[x] = color;
    dot[x] %= fx01_brightness;
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
    if (isInViewport(lx, viewport + fx01_szStackSeg))
      dest[lx] = dot[rightDir ? x : szDot - 1 - x];
  }
}

void stack(CRGB color, CRGB dest[], uint stackStart) {
  for (int x = 0; x < fx01_szStackSeg; x++) {
    dest[stackStart + x] = color;
  }
}

uint incStackSize(int delta) {
  fx01_szStack = capr(fx01_szStack + delta, 0, FRAME_SIZE);
  return fx01_szStack;
}

uint stackAdjust() {
  if (fx01_szStack < fx01_szSegment << 1) {
    incStackSize(fx01_szStackSeg);
    return fx01_szStack;
  }
  if (fx01_cx < 16) {
    shiftRight(frame, FRAME_SIZE, 1);
    incStackSize(-1);
  } else if (inr(fx01_cx, 16, 32) || fx01_cx == fx01_lastCx) {
    incStackSize(-fx01_szStackSeg);
  } else
    incStackSize(fx01_szStackSeg);
  return fx01_szStack;
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
  fx01_szStack = 0;
  fill_solid(frame, FRAME_SIZE, bkg);
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
      int xled = fx01_shuffleIndex[(led + x)%FastLED.size()];
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

//=====================================
void fxa01_setup() {
  FastLED.clear(true);
  fill_solid(dot, MAX_DOT_SIZE, bkg);

  fx01_colors = RainbowColors_p;
  //shuffle led indexes
  shuffleIndexes(fx01_shuffleIndex, NUM_PIXELS);
  reset();
}


void fxa01_run() {
  if (mode == TurnOff) {
    if (turnOff()) {
      reset();
    }
    return;
  }
  fx01_cx = random8();
  fx01_speed = random16(25, 201);
  fx01_szSegment = validateSegmentSize(fx01_szSegment);
  fx01_szStackSeg = fx01_szSegment >> 1;
  int szViewport = FRAME_SIZE - fx01_szStack;
  int startIndex = 0 - fx01_szSegment;

  // Make a dot with current color
  // makeDot(fx01_colors[fx01_cx], fx01_szSegment);
  makeDot(ColorFromPalette(fx01_colors, fx01_cx, random8(fx01_dimmed+24, fx01_brightness), LINEARBLEND), fx01_szSegment);
  for (int led = startIndex; led < szViewport; led++) {
    //move the segment, build up stack
    moveSeg(dot, fx01_szSegment, frame, led, led + 1, szViewport);
    if (led == (szViewport - 1))
      stack(dot[fx01_szSegment - 1], frame, szViewport);

    // Show the updated leds
    //FastLED.show();
    //showFill(frame, FRAME_SIZE);
    moldWindow();
    FastLED.show();

    // Wait a little bit
    delay(fx01_speed);

    //update fx01_speed if in next segment of 10 leds
    if (!fx01_constSpeed && (led % 10 == 9) && random16(0, 2)) {
      fx01_speed = random16(25, 201);
    }
  }

  stackAdjust();
  mode = fx01_szStack == FRAME_SIZE ? TurnOff : Chase;

  //save the color
  fx01_lastCx = fx01_cx;
}

//=====================================
void fxa02_setup() {
  FastLED.clear(true);
  fill_solid(dot, MAX_DOT_SIZE, CRGB::Black);

  fx01_colors = PartyColors_p;
  //shuffle led indexes done by fxa01_setup
  reset();
}

void fxa02_run() {
  if (mode == TurnOff) {
    if (turnOff()) {
      reset();
    }
    return;
  }

  fx01_cx = random8();
  fx01_speed = random16(25, 201);
  fx01_szSegment = random8(2, MAX_DOT_SIZE);
  int szViewport = FRAME_SIZE;
  int startIndex = 0 - fx01_szSegment;

  // Make a dot with current color
  makeDot(ColorFromPalette(fx01_colors, fx01_cx, random8(fx01_dimmed+24, fx01_brightness), LINEARBLEND), fx01_szSegment);
  //move dot to right
  for (int led = startIndex; led < szViewport; led++) {
    //move the segment, build up stack
    moveSeg(dot, fx01_szSegment, frame, led, led + 1, szViewport);
    //retain the last pixel for turning back
    if (led >= (szViewport - 2))
      frame[szViewport - 1] = dot[fx01_szSegment - 1];
    // Show the updated leds
    showFill(frame, FRAME_SIZE);
    // Wait a little bit
    delay(fx01_speed);
    //update fx01_speed if in next segment of 10 leds
    if (!fx01_constSpeed && (led % 10 == 9) && random16(0, 2)) {
      fx01_speed = random16(25, 201);
    }
  }
  //move dot back to left
  for (int led = szViewport; led > startIndex; led--) {
    //move the segment, build up stack
    moveSeg(dot, fx01_szSegment, frame, led, led - 1, szViewport);
    // Show the updated leds
    showFill(frame, FRAME_SIZE);
    // Wait a little bit
    delay(fx01_speed);
    //update fx01_speed if in next segment of 10 leds
    if (!fx01_constSpeed && (led % 10 == 9) && random16(0, 2)) {
      fx01_speed = random16(25, 201);
    }
  }

  //save the color
  fx01_lastCx = fx01_cx;
}
