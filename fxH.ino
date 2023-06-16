///////Engine 1///////
#define EngineStart1 0
#define EngineEnd1 19

///////Engine 2///////
#define EngineStart2 20
#define EngineEnd2 39

///////Engine 3///////
#define EngineStart3 40
#define EngineEnd3 49

///////Engine 4///////
// #define EngineStart4 24
// #define EngineEnd4 32



#define BRIGHTNESS  255
#define FRAMES_PER_SECOND 100

const CRGBPalette16 gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Yellow, CRGB::OrangeRed );

// Define variables used by the sequences.
//uint8_t  thisfade = 8;                                        // How quickly does it fade? Lower = slower fade rate.
//int       thishue = 50;                                       // Starting hue.
uint8_t   thisinc = 1;                                        // Incremental value for rotating hues
//uint8_t   thissat = 100;                                      // The saturation, where 255 = brilliant colours.
//uint8_t   thisbri = 255;                                      // Brightness of a sequence. Remember, max_bright is the overall limiter.
int       huediff = 256;                                      // Range of random #'s to use for hue
//uint8_t thisdelay = 5;                                        // We don't need much delay (if any)

void fxh01_setup() {
}

void fxh01_run()
{
  // Add entropy to random number generator; we use a lot of it. 
  //random16_add_entropy( random8());                                        // <<< use random() for ardunino esp_random() for ESP32

  EVERY_N_MILLISECONDS(250) {
    ENGINE(EngineStart1, EngineEnd1, false);
    ENGINE(EngineStart2, EngineEnd2, false);
    ENGINE(EngineStart3, EngineEnd3, true);
  // ENGINE(EngineSize4,EngineStart4,EngineEnd4);
  
    FastLED.show(); // display this frame
  }
}


// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100 
#define COOLING  20

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 180


///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
void ENGINE(int EngineStart, int EngineEnd, bool gReverseDirection) {
  const int EngineSize = EngineEnd - EngineStart;
  // Array of temperature readings at each simulation cell
  int heat[EngineSize];

  // Step 1.  Cool down every cell a little
    for( int i = 0; i < EngineSize; i++) {
      heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / EngineSize) + 2));
    }
  
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k= EngineSize - 1; k >= 0; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }
    
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if( random8() < SPARKING ) {
       int y = random8(0, EngineSize);
       //int y = random8(EngineStart,EngineEnd);
      heat[y] = qadd8( heat[y], random8(160,255) );
    }

    // Step 4.  Map from heat cells to LED colors
    for( int j = EngineStart; j < EngineEnd; j++) {
      // Scale the heat value from 0-255 down to 0-240 for best results with color palettes.
      uint8_t colorindex = scale8( heat[j - EngineStart], 240);
      CRGB color = ColorFromPalette( gPal, colorindex);
      int pixelnumber = gReverseDirection ? (NUM_PIXELS-1) - j : j;
      leds[pixelnumber] = color;
    }
}
///////////////////////////////////////////////////////////////////////////////////////////

void fxh02_setup() {
  thisfade = 8; 
  thishue = 50;
  thisinc = 1;   
  thissat = 100;
  thisbri = 255;  
  huediff = 256;  
  thisdelay = 5;
}

void fxh02_run () {
  
  fxh02_ChangeMe();                                                 // Check the demo loop for changes to the variables.

  EVERY_N_MILLISECONDS(100) {
    uint8_t maxChanges = 24; 
    nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);   // AWESOME palette blending capability.
  }

  EVERY_N_MILLISECONDS(thisdelay) {                           // FastLED based non-blocking delay to update/display the sequence.
    confetti_pal();
  }
  
  FastLED.show();

} // loop()



void confetti_pal() {                                         // random colored speckles that blink in and fade smoothly

  fadeToBlackBy(leds, NUM_PIXELS, thisfade);                    // Low values = slower fade.
  int pos = random16(NUM_PIXELS);                               // Pick an LED at random.
  leds[pos] = ColorFromPalette(currentPalette, thishue + random16(huediff)/4 , thisbri, currentBlending);
  thishue = thishue + thisinc;                                // It increments here.
  
} // confetti_pal()



void fxh02_ChangeMe() {                                             // A time (rather than loop) based demo sequencer. This gives us full control over the length of each sequence.
  
  uint8_t secondHand = (millis() / 1000) % 15;                // IMPORTANT!!! Change '15' to a different value to change duration of the loop.
  static uint8_t lastSecond = 99;                             // Static variable, means it's only defined once. This is our 'debounce' variable.
  if (lastSecond != secondHand) {                             // Debounce to make sure we're not repeating an assignment.
    lastSecond = secondHand;
    switch(secondHand) {
      case  0: targetPalette = OceanColors_p; thisinc=1; thishue=192; thissat=255; thisfade=2; huediff=255; break;  // You can change values here, one at a time , or altogether.
      case  5: targetPalette = LavaColors_p; thisinc=2; thishue=128; thisfade=8; huediff=64; break;
      case 10: targetPalette = ForestColors_p; thisinc=1; thishue=random16(255); thisfade=1; huediff=16; break;      // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
      case 15: break;                                         // Here's the matching 15 for the other one.
    }
  }
  
} // fxh02_ChangeMe()