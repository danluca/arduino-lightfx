// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_GLOBAL_H
#define ARDUINO_LIGHTFX_GLOBAL_H

#define capd(x, d) (((x)<=(d))?(d):(x))
#define capu(x, u) (((x)>=(u))?(u):(x))
#define capr(x, d, u) (capu(capd(x,d),u))
#define inr(x, d, u) (((x)>=(d))&&((x)<(u)))
#define inc(x, i, u) ((x+i)%(u))
#define incr(x, i, u) x=(x+i)%(u)
#define arrSize(A) (sizeof(A) / sizeof((A)[0]))
#define qsubd(x, b) (((x)>(b))?(b):0)                               // Clip. . . . A digital unsigned subtraction macro. if result <0, then x=0. Otherwise, x=b.
#define qsuba(x, b) (((x)>(b))?(x-b):0)                             // Level shift. . . Unsigned subtraction macro. if result <0, then x=0. Otherwise x=x-b.
#define asub(a, b)  (((a)>(b))?(a-b):(b-a))

#define LED_EFFECT_ID_SIZE  6
#define MAX_EFFECTS_HISTORY 20
#define AUDIO_HIST_BINS_COUNT   10
#define FX_SLEEPLIGHT_ID    "FXA6"

/**
 * Add one byte to another, saturating at given cap value
 * @param i operand to add
 * @param j operand to add
 * @param cap upper bound
 * @return the result of adding i to j saturating at cap
 */
static uint8_t cadd8(const uint8_t i, const uint8_t j, const uint8_t cap) {
    uint t = i + j;
    if (t > cap)
        t = cap;
    return t;
}

/**
 * Subtract second byte arg from the first, saturating at given cap value
 * @param i operand to subtract from
 * @param j operand to subtract
 * @param cap lower bound
 * @return the result of i-j saturating at cap
 */
static uint8_t csub8(const uint8_t i, const uint8_t j, const uint8_t cap) {
    int t = i - j;
    if (t < cap)
        t = cap;
    return t;
}

#endif //ARDUINO_LIGHTFX_GLOBAL_H
