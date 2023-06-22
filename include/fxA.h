//
// Created by Dan on 06.21.2023.
//

#ifndef LIGHTFX_FXA_H
#define LIGHTFX_FXA_H

#include "efx_setup.h"

const char fxa_description[] = "Moving a fixed size segment with variable speed and stacking at the end - Tetris simplified";
const char fxb_description[] = "Moving a 1 pixel segment with constant speed and stacking at the end; flickering; Halloween colors - Halloween Tetris";

const uint MAX_DOT_SIZE = 16;
const uint8_t fxa_brightness = 175;
const uint8_t fxa_dimmed = 20;
const CRGB bkg = CRGB::Black;
const uint FRAME_SIZE = 19;
const uint turnOffSeq[] = { 1, 1, 2, 3, 5, 7, 10 };
enum OpMode { TurnOff, Chase };
const CRGBPalette16 fxb_colors = CRGBPalette16(CRGB::Red, CRGB::Purple, CRGB::Orange, CRGB::Black);
const uint8_t fxd_brightness = 140;

extern CRGBPalette16 fxa_colors;
extern uint fxa_cx;
extern uint fxa_lastCx;
extern uint fxa_speed;
extern uint fxa_szSegment;
extern uint fxa_szStackSeg;
extern uint fxa_szStack;
extern bool fxa_constSpeed;
extern OpMode mode;
extern int fxa_shuffleIndex[NUM_PIXELS];
extern CRGB dot[MAX_DOT_SIZE];
extern CRGB frame[NUM_PIXELS];
extern uint curPos;

CRGB* makeDot(CRGB color, uint szDot);
bool isInViewport(int ledIndex, int viewportSize = FRAME_SIZE);
bool isVisible(int ledIndex);
uint validateSegmentSize(uint segSize);
void moveSeg(const CRGB dot[], uint szDot, CRGB dest[], uint lastPos, uint newPos, uint viewport);
void stack(CRGB color, CRGB dest[], uint stackStart);
uint fxa_incStackSize(int delta, uint max);
uint fxa_stackAdjust(CRGB array[], uint szArray);
void moldWindow();
void reset();
bool turnOff();


#endif //LIGHTFX_FXA_H
