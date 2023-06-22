//
// Created by Dan on 06.21.2023.
//

#ifndef LIGHTFX_FXB_H
#define LIGHTFX_FXB_H

#include "efx_setup.h"
//typedef void (*SimplePatternList[])();  // List of patterns to cycle through.  Each is defined as a separate function below.

extern uint8_t gHue;
extern uint8_t max_bright;
extern CRGBPalette16 palette;
extern CRGBPalette16 targetPalette;


void fadein();
void rainbow();
void rainbowWithGlitter();
void addGlitter(fract8 chanceOfGlitter);
void fxb_confetti();
void sinelon();
void bpm();
void juggle();
void ease();



#endif //LIGHTFX_FXB_H
