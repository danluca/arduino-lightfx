//
// Created by Dan on 06.21.2023.
//

#ifndef LIGHTFX_FXE_H
#define LIGHTFX_FXE_H

#include "efx_setup.h"
#include "fxD.h"
#include "fxB.h"

#define qsubd(x, b) ((x>b)?b:0)                               // Clip. . . . A digital unsigned subtraction macro. if result <0, then x=0. Otherwise, x=b.
#define qsuba(x, b) ((x>b)?x-b:0)                             // Level shift. . . Unsigned subtraction macro. if result <0, then x=0. Otherwise x=x-b.


// Global variables can be changed on the fly.
//uint8_t max_bright = 128;                                      // Overall brightness.

// Palette definitions
extern CRGBPalette16 currentPalette;
extern TBlendType    currentBlending;                                // NOBLEND or LINEARBLEND

// Define variables used by the sequences.
extern int      twinkrate;
extern bool       randhue;


void twinkle();
void e01_ChangeMe();
void beatwave();



#endif //LIGHTFX_FXE_H
