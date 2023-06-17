#include "config.h"

#define capd(x,d) ((x<d)?d:x)
#define capu(x,u) ((x>u)?u:x)
#define capr(x,d,u) (capu(capd(x,d),u))
#define inr(x,d,u) ((x>=d)&&(x<u))

#define MAX_NUM_PIXELS  1024    //maximum number of pixels supported (equivalent of 330ft LED strips). If more are needed, we'd need to revisit memory allocation and PWM timings

//#define NUM_PIXELS  305    //number of pixels on the house edge strip
#define NUM_PIXELS  50    //number of pixels on a single 16.5ft (5m) strip

CRGB leds[NUM_PIXELS];

// Effectx setup and run functions - two arrays of same(!) size
void (*const setupFunc[])() = {fxa01_setup, fxa02_setup, fxb01_setup, fxc01_setup, fxc02_setup, fxd01_setup, fxd02_setup, fxe01_setup, fxe02_setup, fxh01_setup, fxh02_setup};
void (*const fxrunFunc[])() = {fxa01_run,   fxa02_run,   fxb01_run,   fxc01_run,   fxc02_run,   fxd01_run,   fxd02_run,   fxe01_run,   fxe02_run,   fxh01_run,   fxh02_run};
const uint szFx = sizeof(setupFunc)/sizeof(void*);

// current effect
uint curFxIndex = 0;
uint lastFxIndex = curFxIndex;
ulong fxSwitchTime = 0;
bool autoSwitch = true;

//~ Support functions -----------------
void setupStateLED() {
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  stateLED(CRGB::Black);
}

void stateLED(CRGB color) {
  analogWrite(LEDR, 255-color.r);
  analogWrite(LEDG, 255-color.g);
  analogWrite(LEDB, 255-color.b);
}

void ledStripInit() {
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_PIXELS).setCorrection(TypicalSMD5050).setTemperature(Tungsten100W);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);
}

void showFill(const CRGB frame[], uint szFrame);

void shiftRight(CRGB arr[], int szArr, uint pos) {
  CRGB seed = arr[0];
  if (pos >= szArr) {
    fill_solid(arr, szArr, seed);
    return;
  }
  if (pos == 0) {
    return;
  }
  for (int x = szArr - 1; x >= 0; x--) {
    if (x < pos) {
      arr[x] = seed;
    } else {
      arr[x] = arr[x - pos];
    }
  }
}

void shiftLeft(CRGB arr[], int szArr, uint pos) {
  CRGB seed = arr[szArr - 1];
  if (pos >= szArr) {
    fill_solid(arr, szArr, seed);
    return;
  }
  if (pos == 0) {
    return;
  }
  for (int x = 0; x < szArr; x++) {
    if (x + pos >= szArr) {
      arr[x] = seed;
    } else {
      arr[x] = arr[x + pos];
    }
  }
}

void shuffleIndexes(int array[], uint szArray) {
  //shuffle LED indexes
  for (int x = 0; x < szArray; x++) {
    array[x] = x;
  }
  uint swIter = (szArray>>1)+(szArray>>3);
  for (int x = 0; x < swIter; x++) {
    int r = random16(0, szArray);
    int tmp = array[x];
    array[x] = array[r];
    array[r] = tmp;
  }
}

void smoothOffOne(CRGB arr[], uint szArr, int xLed) {
  for (int fade = 0; fade < 8; fade++) {
    arr[capr(xLed, 0, szArr)].nscale8(255 >> fade);
    //FastLED.show();
    showFill(arr, szArr);
    delay(125);
  }
}

void smoothOffMultiple(CRGB arr[], uint szArr, int xLed[], int szLed) {
  for (int fade = 0; fade < 8; fade++) {
    for (int x = 0; x < szLed; x++) {
      arr[capr(xLed[x], 0, szArr)].nscale8(255 >> fade);
    }
    //FastLED.show();
    if (arr == leds) 
      FastLED.show();
    else
      showFill(arr, szArr);
    delay(100);
  }
}

void offTrailColor(CRGB arr[], int x) {
  if (x < 1) {
    arr[x] = CRGB::Black;
    return;
  }
  arr[x] = arr[x - 1];
  arr[x - 1] = CRGB::Black;
}

void setTrailColor(CRGB arr[], int x, CRGB color, uint8_t dotBrightness, uint8_t trailBrightness) {
  arr[x] = color;
  arr[x] %= dotBrightness;
  if (x < 1) {
    return;
  }
  arr[x - 1] = color.nscale8_video(trailBrightness);
}

//copy arrays using memcpy (arguably fastest way) - no checks are made on the length copied vs actual length of both arrays
void copyArray(CRGB* src, CRGB* dest, size_t length) {
  memcpy(dest, src, sizeof(src[0])*length);
}

// copy arrays using pointer loops - one of the faster ways. No checks are made on the validity of offsets, length for both arrays
void copyArray(const CRGB* src, uint srcOfs, CRGB* dest, uint destOfs, size_t length) {
  const CRGB* srSt = &src[srcOfs];
  CRGB* dsSt = &dest[destOfs];
  for (int x=0; x<length; x++) {
    *dsSt++ = *srSt++;
  }
}

bool isAnyLedOn(CRGB arr[], uint szArray, CRGB backg) {
  for (int x=0; x<szArray; x++) {
    if (arr[x] != backg)
      return true;
  }
  return false;
}

void fillArray(CRGB* src, size_t szSrc, CRGB color) {
  fill_solid(src, szSrc, color);
}

void fillArray(const CRGB* src, size_t srcLength, CRGB* array, size_t arrLength, uint arrOfs=0) {
  size_t curFrameIndex = arrOfs;
  while (curFrameIndex < arrLength) {
    size_t len = capu(curFrameIndex+srcLength, arrLength)-curFrameIndex;
    copyArray(src, 0, array, curFrameIndex, len);
    curFrameIndex += srcLength;
  }
}

void showFill(const CRGB frame[], uint szFrame) {
  if (frame != NULL)
    fillArray(frame, szFrame, FastLED.leds(), FastLED.size());
  FastLED.show();
}

void pushFrame(const CRGB frame[], uint szFrame, uint ofsDest=0, bool repeat=false) {
  CRGB* strip = FastLED.leds();
  int szStrip = FastLED.size();
  if (repeat)
    fillArray(frame, szFrame, strip, szStrip, ofsDest);
  else {
    size_t len = capd(ofsDest+szFrame, szStrip)-ofsDest;
    copyArray(frame, 0, strip, ofsDest, len);
  }
}

CRGB* mirrorLow(CRGB array[], uint szArray) {
  uint swaps = szArray>>1;
  for (int x=0; x<swaps; x++) {
    array[szArray-x-1] = array[x];
  }
  return array;
}

CRGB* mirrorHigh(CRGB array[], uint szArray) {
  uint swaps = szArray>>1;
  for (int x=0; x<swaps; x++) {
    array[x] = array[szArray-x-1];
  }
  return array;
}

CRGB* reverseArray(CRGB array[], uint szArray) {
  uint swaps = szArray>>1;
  for (int x=0; x<swaps; x++) {
    CRGB tmp = array[x];
    array[x] = array[szArray-x-1];
    array[szArray-x-1] = tmp;
  }
  return array;
}

CRGB* cloneArray(const CRGB src[], CRGB dest[], size_t length) {
  copyArray(src, 0, dest, 0, length);
  return dest;
}

//Setup all effects -------------------
void fx_setup() {
  ledStripInit();
  random16_set_seed(millis());

  //initialize the effects configured in the functions above
  for (int f = 0; f < szFx; f++) {
    setupFunc[f]();
  }
}

//Run currently selected effect -------
void fx_run() {
  if (autoSwitch) {
    if ((millis()-fxSwitchTime)>300000)
      curFxIndex = random8(0, szFx);
      // curFxIndex = capr(curFxIndex+1,0,szFx-1);
  } else 
    curFxIndex = capr(curFxIndex, 0, szFx-1);
  //if effect has changed, re-run the effect's setup
  if (curFxIndex != lastFxIndex) {
    setupFunc[curFxIndex]();
    fxSwitchTime = millis();
    logInfo("Switched to effect %d at %lu", curFxIndex, fxSwitchTime);
  }
  //run the effect
  fxrunFunc[curFxIndex]();
  lastFxIndex = curFxIndex;
  yield();
}
