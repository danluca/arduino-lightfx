/**
 * Tetris flavor 1 - single dot/segment moving one direction, random color, stack at the end, configurable various speed between segments of 8
 * Couple of definitions: physical ledstrip has the first led at index 0. Virtual ledstrip has any length and can have indexes both <0 and >number of leds.
 * Viewport is the ledstrip between index 0 and the start of the stack. Stack is the portion of the ledstrip where the segments accumulate, there is no movement in the stack.
 */

const char fx01_description[] = "Moving a fixed size segment with variable speed and stacking at the end - Tetris simplified";
// const CRGB fx01_colors[] = {CRGB::Blue, CRGB::Red, CRGB::Lime, CRGB::Yellow, CRGB::Purple, CRGB::OrangeRed, CRGB::White, CRGB::Sienna, CRGB::SkyBlue, CRGB::FairyLight, CRGB::Green};
CRGBPalette16 fx01_colors = RainbowColors_p;
uint fx01_cx = 0;
uint fx01_lastCx = 0;
uint fx01_speed = 100;
const uint MAX_DOT_SIZE = 16;
const uint8_t fx01_brightness = 175;
const uint8_t fx01_dimmed = 20;
// const uint FX01_NUM_COLORS = sizeof(fx01_colors)/sizeof(CRGB);
const uint FX01_NUM_COLORS = 16;
const CRGB bkg = CRGB::Black;
const uint FX01_STACK_PUSH_COLOR = 7;

uint fx01_szSegment = 4;
uint fx01_szStackSeg = fx01_szSegment >> 1;
uint fx01_szStack = 0;
CRGB dot[MAX_DOT_SIZE];
int fx01_shuffleIndex[NUM_PIXELS];
bool fx01_constSpeed = false;

CRGB* makeDot(CRGB color, uint szDot) {
  dot[0] = color;
  dot[0] %= fx01_dimmed;
  for (uint x = 1; x < szDot; x++) {
    dot[x] = color;
    dot[x] %= fx01_brightness;
  }
  return dot;
}

bool isInViewport(int ledIndex, int viewportSize = NUM_PIXELS) {
  int viewportLow = 0;
  int viewportHi = viewportSize;
  return (ledIndex >= viewportLow) && (ledIndex < viewportHi);
}

bool isVisible(int ledIndex) {
  return (ledIndex>=0) && (ledIndex<NUM_PIXELS);
}

uint validateSegmentSize(uint segSize) {
  return max(min(segSize, MAX_DOT_SIZE), 2);
}

void moveSeg(const CRGB dot[], uint szDot, uint lastPos, uint newPos, uint viewport) {
  bool rightDir = newPos >= lastPos;
  int bkgSeg = min(szDot, abs(newPos-lastPos));
  for (int x=0; x<bkgSeg; x++) {
    int lx = rightDir?lastPos+x:newPos+szDot+x;
    if (!isVisible(lx))
      continue;
    if (isInViewport(lx, viewport))
      leds[lx] = bkg;
  }
  for (int x=0; x<szDot; x++) {
    int lx = newPos+x;
    if (!isVisible(lx))
      continue;
    if (isInViewport(lx, viewport+fx01_szStackSeg))
      leds[lx] = dot[rightDir?x:szDot-1-x];
  }
}

void stack(CRGB color, uint stackStart) {
  for (int x=0; x<fx01_szStackSeg;x++) {
    leds[stackStart+x]=color;
  }
}

uint incStackSize(int delta) {
  fx01_szStack = (fx01_szStack+delta)%NUM_PIXELS;
  return fx01_szStack;
}

uint stackAdjust() {
  if ((fx01_szStack>=fx01_szSegment) && (fx01_cx==FX01_STACK_PUSH_COLOR || fx01_cx==fx01_lastCx)) {
    if (fx01_cx == FX01_STACK_PUSH_COLOR) {
      shiftRight(leds, NUM_PIXELS, 1);
      incStackSize(-1);
      random16_add_entropy(millis() >> 4);
      FastLED.show();
      delay(fx01_speed << 2);
    } else if (fx01_cx == fx01_lastCx) {
      incStackSize(-fx01_szStackSeg);
    } 
  } else {
    incStackSize(fx01_szStackSeg);
  }
  return fx01_szStack;
}

void clearSmooth() {
  if (!isAnyLedOn(bkg)) 
    return;
  
  for (int led = 0; led < NUM_PIXELS;) {
    int szThisRound = random16(1, 6);
    if (led + szThisRound > NUM_PIXELS) {
      szThisRound = NUM_PIXELS - led;
    }
    int thisRound[szThisRound];
    for (int x = 0; x < szThisRound; x++) {
      thisRound[x] = fx01_shuffleIndex[led++];
    }
    smoothOffMultiple(thisRound, szThisRound);
  }
}

//=====================================
void fxa01_setup() {
  fill_solid(leds, NUM_PIXELS, CRGB::Black);
  fill_solid(dot, MAX_DOT_SIZE, CRGB::Black);

  fx01_colors = RainbowColors_p;
  //shuffle led indexes
  shuffleIndexes(fx01_shuffleIndex, NUM_PIXELS);
  fx01_szStack = 0;
  clearSmooth();
}


void fxa01_run() {
  // fx01_cx = random16(0, FX01_NUM_COLORS);
  fx01_cx = random8();
  fx01_speed = random16(25, 201);
  fx01_szSegment = validateSegmentSize(fx01_szSegment);
  fx01_szStackSeg = fx01_szSegment >> 1;
  int szViewport = NUM_PIXELS-fx01_szStack;
  int startIndex = 0-fx01_szSegment;

  // Make a dot with current color
  // makeDot(fx01_colors[fx01_cx], fx01_szSegment);
  makeDot(ColorFromPalette(fx01_colors, fx01_cx, random8(30, 201), LINEARBLEND), fx01_szSegment);
  for (int led = startIndex; led < szViewport; led++) {
    //move the segment, build up stack
    moveSeg(dot, fx01_szSegment, led, led+1, szViewport);
    if (led==(szViewport-1))
      stack(dot[fx01_szSegment-1], szViewport);

    // Show the updated leds
    FastLED.show();

    // Wait a little bit
    delay(fx01_speed);
    
    //update fx01_speed if in next segment of 10 leds
    if (!fx01_constSpeed && (led % 10 == 9) && random16(0, 2)) {
      fx01_speed = random16(25, 201);
    }
  }

  if (fx01_szStack == 0) {
    clearSmooth();
    incStackSize(fx01_szStackSeg);
  } else
    stackAdjust();
  
  //save the color
  fx01_lastCx = fx01_cx;
}

//=====================================
void fxa02_setup() {
  fill_solid(leds, NUM_PIXELS, CRGB::Black);
  fill_solid(dot, MAX_DOT_SIZE, CRGB::Black);

  fx01_colors = PartyColors_p;
  //shuffle led indexes
  shuffleIndexes(fx01_shuffleIndex, NUM_PIXELS);
  fx01_szStack = 0;
  clearSmooth();
}

void fxa02_run() {
  // fx01_cx = random16(0, FX01_NUM_COLORS);
  fx01_cx = random8();
  fx01_speed = random16(25, 201);
  fx01_szSegment = random8(2, MAX_DOT_SIZE);
  // fx01_szSegment = validateSegmentSize(fx01_szSegment);
  int szViewport = NUM_PIXELS;
  int startIndex = 0-fx01_szSegment;

  // Make a dot with current color
  // makeDot(fx01_colors[fx01_cx], fx01_szSegment);
  makeDot(ColorFromPalette(fx01_colors, fx01_cx, random8(30, 201), LINEARBLEND), fx01_szSegment);
  //move dot to right
  for (int led = startIndex; led < szViewport; led++) {
    //move the segment, build up stack
    moveSeg(dot, fx01_szSegment, led, led+1, szViewport);
    //retain the last pixel for turning back
    if (led>=(szViewport-2))
      leds[szViewport-1] = dot[fx01_szSegment-1];
    // Show the updated leds
    FastLED.show();
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
    moveSeg(dot, fx01_szSegment, led, led-1, szViewport);
    // Show the updated leds
    FastLED.show();
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
