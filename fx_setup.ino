// Effectx setup and run functions - two arrays of same(!) size

#define capd(x,d) ((x<=d)?d:x)
#define capu(x,u) ((x>u)?u:x)
#define capr(x,d,u) (capu(capd(x,d),u))

void (*setupFunc[])() = {fxa01_setup, fxa02_setup, fxb01_setup, fxc01_setup, fxc02_setup, fxd01_setup, fxd02_setup, fxe01_setup, fxe02_setup};
void (*fxrunFunc[])() = {fxa01_run,   fxa02_run,   fxb01_run,   fxc01_run,   fxc02_run,   fxd01_run,   fxd02_run,   fxe01_run,   fxe02_run};
const uint szFx = sizeof(setupFunc)/sizeof(void*);
// current effect
uint curFxIndex = 6;
uint lastFxIndex = curFxIndex;
ulong fxSwitchTime = 0;
bool autoSwitch = false;

//TODO: how to have repeat segments on the led strip

//setup all effects
void fx_setup() {
  random16_set_seed(millis());

  //initialize the effects configured in the functions above
  for (int f = 0; f < szFx; f++) {
    setupFunc[f]();
  }
}

//run currently selected effect
void fx_run() {
  if (autoSwitch) {
    if ((millis()-fxSwitchTime)>300000)
      curFxIndex = random8(0, szFx);
  } else 
    curFxIndex = capr(curFxIndex, 0, szFx);
  //if effect has changed, re-run the effect's setup
  if (curFxIndex != lastFxIndex) {
    setupFunc[curFxIndex]();
    fxSwitchTime = millis();
  }
  //run the effect
  fxrunFunc[curFxIndex]();
  lastFxIndex = curFxIndex;
  yield();
}

//utilities
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
  for (int x = 0; x < szArray; x++) {
    int r = random16(0, szArray);
    int tmp = array[x];
    array[x] = array[r];
    array[r] = tmp;
  }
}

void smoothOffOne(int xLed) {
  for (int fade = 0; fade < 8; fade++) {
    leds[xLed].nscale8(255 >> fade);
    FastLED.show();
    delay(125);
  }
}

void smoothOffMultiple(int xLed[], int szLed) {
  for (int fade = 0; fade < 8; fade++) {
    for (int x = 0; x < szLed; x++) {
      leds[xLed[x]].nscale8(255 >> fade);
    }
    FastLED.show();
    delay(100);
  }
}

void offTrailColor(int x) {
  if (x < 1) {
    leds[x] = CRGB::Black;
    return;
  }
  leds[x] = leds[x - 1];
  leds[x - 1] = CRGB::Black;
}

void setTrailColor(int x, CRGB color, uint8_t dotBrightness, uint8_t trailBrightness) {
  leds[x] = color;
  leds[x] %= dotBrightness;
  if (x < 1) {
    return;
  }
  leds[x - 1] = color.nscale8_video(trailBrightness);
}

//copy arrays using memcpy (arguably fastest way) - no checks are made on the length copied vs actual length of both arrays
void copyArray(CRGB* src, CRGB* dest, size_t length) {
  memcpy(dest, src, sizeof(src[0])*length);
}

// copy arrays using pointer loops - one of the faster ways. No checks are made on the validity of offsets, length for both arrays
void copyArray(CRGB* src, uint srcOfs, CRGB* dest, uint destOfs, size_t length) {
  CRGB* srSt = &src[srcOfs];
  CRGB* dsSt = &dest[destOfs];
  for (int x=0; x<length; x++) {
    *dsSt++ = *srSt++;
  }
}

bool isAnyLedOn(CRGB backg) {
  for (int x=0; x<NUM_PIXELS; x++) {
    if (leds[x] != backg)
      return true;
  }
  return false;
}

