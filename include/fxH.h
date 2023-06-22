//
// Created by Dan on 06.21.2023.
//

#ifndef LIGHTFX_FXH_H
#define LIGHTFX_FXH_H

#include "efx_setup.h"
#include "fxE.h"

#define MAX_ENGINE_SIZE 40
///////Fire 1///////
#define FireStart1 0
#define FireEnd1 19

///////Fire 2///////
#define FireStart2 20
#define FireEnd2 39

///////Fire 3///////
#define FireStart3 40
#define FireEnd3 50


#define FRAMES_PER_SECOND 10

const CRGBPalette16 gPal = CRGBPalette16(CRGB::Black, CRGB::Red, CRGB::OrangeRed, CRGB::Yellow);
const uint8_t flameBrightness = 180;
const uint8_t sparkBrightness = 255;

//
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation,
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100
#define COOLING 75

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 150

extern int fire1[MAX_ENGINE_SIZE];
extern int fire2[MAX_ENGINE_SIZE];
extern int fire3[MAX_ENGINE_SIZE];


void Fire2012WithPalette(int heat[], const uint szArray, const uint stripOffset, bool reverse);
void confetti_pal();
void fxh02_ChangeMe();




#endif //LIGHTFX_FXH_H
