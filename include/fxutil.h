// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef FXUTIL_H
#define FXUTIL_H

#include <Arduino.h>
#include <FastLED.h>
#include "constants.hpp"

/**
 * Viewport definition packed as a 4-byte unsigned integer - uint32_t
 */
struct Viewport {
    uint16_t low;
    uint16_t high;

    explicit Viewport(uint16_t high);
    Viewport(uint16_t low, uint16_t high);
    [[nodiscard]] uint16_t size() const;
};

namespace fx {
    void shiftRight(CRGBSet &set, CRGB feedLeft, Viewport vwp = static_cast<Viewport>(0), uint16_t pos = 1);

    void loopRight(CRGBSet &set, Viewport vwp = static_cast<Viewport>(0), uint16_t pos = 1);

    void shiftLeft(CRGBSet &set, CRGB feedRight, Viewport vwp = static_cast<Viewport>(0), uint16_t pos = 1);

    bool spreadColor(CRGBSet &set, CRGB color = BKG, uint8_t gradient = 255);

    bool moveBlend(CRGBSet &target, const CRGBSet &segment, fract8 overlay, uint16_t fromPos, uint16_t toPos);
    bool areSame(const CRGBSet &lhs, const CRGBSet &rhs);

    void shuffleIndexes(uint16_t array[], uint16_t szArray);
    void shuffle(CRGBSet &set);

    uint16_t easeOutBounce(uint16_t x, uint16_t lim);
    uint16_t easeOutQuad(uint16_t x, uint16_t lim);

    void copyArray(const CRGB *src, CRGB *dest, uint16_t length);

    void copyArray(const CRGB *src, uint16_t srcOfs, CRGB *dest, uint16_t destOfs, uint16_t length);

    uint16_t countPixelsBrighter(const CRGBSet *set, CRGB backg = BKG);

    bool isAnyLedOn(CRGBSet *set, CRGB backg = BKG);

    bool isAnyLedOn(const CRGB *arr, uint16_t szArray, CRGB backg = BKG);

    void fillArray(const CRGB *src, uint16_t srcLength, CRGB *array, uint16_t arrLength, uint16_t arrOfs = 0);

    void replicateSet(const CRGBSet& src, CRGBSet& dest);

    uint8_t adjustStripBrightness();

    void mirrorLow(CRGBSet &set);

    void mirrorHigh(CRGBSet &set);

    CRGB& setBrightness(CRGB& rgb, uint8_t br);
    uint8_t getBrightness(const CRGB& rgb);

    inline CHSV toHSV(const CRGB &rgb) { return rgb2hsv_approximate(rgb); }
    inline CRGB toRGB(const CHSV &hsv) { CRGB rgb{}; hsv2rgb_rainbow(hsv, rgb); return rgb; }

    bool rblend(CRGB &existing, const CRGB &target, fract8 frOverlay);
    void blendMultiply(CRGBSet &blendLayer, const CRGBSet &topLayer);
    void blendMultiply(CRGB &blendRGB, const CRGB &topRGB);
    void blendScreen(CRGBSet &blendLayer, const CRGBSet &topLayer);
    void blendScreen(CRGB &blendRGB, const CRGB &topRGB);
    void blendOverlay(CRGBSet &blendLayer, const CRGBSet &topLayer);
    void blendOverlay(CRGB &blendRGB, const CRGB &topRGB);
    CRGB adjustBrightness(CRGB color, uint8_t bright);


}

#endif //FXUTIL_H
