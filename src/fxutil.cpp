// Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
//
#include "fxutil.h"
#include "efx_setup.h"
#include "FxSchedule.h"
#include "sysinfo.h"
#include "TimeLib.h"
#include "util.h"

// Viewport
Viewport::Viewport(const uint16_t high) : Viewport(0, high) {}

Viewport::Viewport(const uint16_t low, const uint16_t high) : low(low), high(high) {}

uint16_t Viewport::size() const {
    return qsuba(high, low);
}

// Fx Utilities
/**
 * Ease Out Bounce implementation - leverages the double precision original implementation converted to int in a range
 * @param x input value
 * @param lim high limit range
 * @return the result in [0,lim] inclusive range
 * @see https://easings.net/#easeOutBounce
 */
uint16_t fx::easeOutBounce(const uint16_t x, const uint16_t lim) {
    static constexpr float d1 = 2.75f;
    static constexpr float n1 = 7.5625f;

    const float xf = static_cast<float>(x)/static_cast<float>(lim);
    float res = 0;
    if (xf < 1/d1) {
        res = n1*xf*xf;
    } else if (xf < 2/d1) {
        const float xf1 = xf - 1.5f/d1;
        res = n1*xf1*xf1 + 0.75f;
    } else if (xf < 2.5f/d1) {
        const float xf1 = xf - 2.25f/d1;
        res = n1*xf1*xf1 + 0.9375f;
    } else {
        const float xf1 = xf - 2.625f/d1;
        res = n1*xf1*xf1 + 0.984375f;
    }
    return static_cast<uint16_t>(res * static_cast<float>(lim));
}

/**
 * Ease Out Quad implementation - leverages the double-precision original implementation converted to int in a range
 * @param x input value
 * @param lim high limit range
 * @return the result in [0,lim] inclusive range
 * @see https://easings.net/#easeOutQuad
 */
uint16_t fx::easeOutQuad(const uint16_t x, const uint16_t lim) {
    const auto limf = static_cast<float>(lim);
    const float xf = static_cast<float>(x)/limf;
    return static_cast<uint16_t>((1 - (1 - xf) * (1 - xf)) * limf);
}

/**
 * Shifts the content of an array to the right by the number of positions specified
 * First item of the array (arr[0]) is used as seed to fill the new elements entering left
 * @param set pixel set to shift
 * @param feedLeft the color to introduce from the left as we shift the array
 * @param vwp limits of the shifting area
 * @param pos how many positions to shift right
 */
void fx::shiftRight(CRGBSet &set, const CRGB feedLeft, Viewport vwp, const uint16_t pos) {
    if ((pos == 0) || (set.size() == 0) || (vwp.low >= set.size()))
        return;
    if (vwp.size() == 0)
        vwp = static_cast<Viewport>(set.size());
    if (pos >= vwp.size()) {
        const uint16_t hiMark = capu(vwp.high, set.size());
        set(vwp.low, hiMark) = feedLeft;
        return;
    }
    const uint16_t hiMark = capu(vwp.high, set.size());
    //don't use >= as the indexer is unsigned and always >=0 --> infinite loop
    for (uint16_t x = hiMark; x > vwp.low; x--) {
        const uint16_t y = x - 1;
        set[y] = y < pos ? feedLeft : set[y-pos];
    }
}

/**
 * Shifts the contents of an CRGBSet to the right in a circular buffer manner - the elements falling off to the right
 * are entering through the left
 * @param set the set
 * @param vwp limits of the shifting area
 * @param pos how many positions to shift right
 */
void fx::loopRight(CRGBSet &set, Viewport vwp, uint16_t pos) {
    if ((pos == 0) || (set.size() == 0) || (vwp.low >= set.size()))
        return;
    if (vwp.size() == 0)
        vwp = static_cast<Viewport>(set.size());
    const uint16_t hiMark = capu(vwp.high, set.size());
    pos = pos % vwp.size();
    if (pos == 0)
        return;
    auto rev = [&](uint16_t lo, uint16_t hi){ while (lo < hi) { const CRGB tmp = set[lo]; set[lo] = set[hi]; set[hi] = tmp; ++lo; --hi; } };
    const uint16_t lo = vwp.low;
    const uint16_t hi = hiMark - 1;
    rev(lo, hi);
    rev(lo, lo + pos - 1);
    rev(lo + pos, hi);
}

/**
 * Shifts the content of an array to the left by the number of positions specified
 * The elements entering right are filled with current's array-last value (arr[szArr-1])
 * @param set pixel set to shift
 * @param feedRight the color to introduce from the right as we shift the array
 * @param vwp limits of the shifting area
 * @param pos how many positions to shift left
 */
void fx::shiftLeft(CRGBSet &set, const CRGB feedRight, Viewport vwp, const uint16_t pos) {
    if ((pos == 0) || (set.size() == 0) || (vwp.low >= set.size()))
        return;
    if (vwp.size() == 0)
        vwp = static_cast<Viewport>(set.size());
    const uint16_t hiMark = capu(vwp.high, (set.size()-1));
    if (pos >= vwp.size()) {
        set(vwp.low, hiMark) = feedRight;
        return;
    }
    for (uint16_t x = vwp.low; x <= hiMark; x++) {
        const uint16_t y = x + pos;
        set[x] = y < set.size() ? set[y] : feedRight;
    }
}

/**
 * Spread the color provided into the pixel set starting from the left, in gradient steps
 * Note that while the color is spread, the rest of the pixels in the set remain in place - this is
 * different from shifting the set while feeding the color from one end
 * @param set LED strip to update
 * @param color the color to spread
 * @param gradient amount of gradient to use per each spread call
 * @return true when all pixels in the set are at full spread color value
 */
bool fx::spreadColor(CRGBSet &set, const CRGB color, const uint8_t gradient) {
    uint16_t clrPos = 0;
    while (clrPos < set.size()) {
        if (set[clrPos] != color)
            break;
        clrPos++;
    }

    if (clrPos == set.size())
        return true;

    //the nblend takes care of the edge cases of gradient 0 and 255. When 0 it returns the source color unchanged;
    //when 255 it replaces the source color entirely with the overlay
    nblend(set[clrPos], color, gradient);
    return false;
}

/**
 * Moves the segment from old position to new position by leveraging blend, for a smooth transition
 * @param target pixel set that receives the move - this is (much) larger than the segment
 * @param segment the segment to move will not be changed - this is (much) smaller than the target
 * @param overlay amount of overlay to apply
 * @param fromPos old position - the index where the segment is currently at in the target pixel set. The index is the next position from the END of the segment.
 *  Ranges from 0 to target.size()+segment.size()
 * @param toPos new position - the index where the segment needs to be moved at in the target pixel set. The index is the next position from the END of the segment.
 *  Ranges from 0 to target.size()+segment.size()
 * @return true when the move is complete, that is when the segment at a new position in the target set matches the original segment completely (has blended)
 */
bool fx::moveBlend(CRGBSet &target, const CRGBSet &segment, const fract8 overlay, const uint16_t fromPos, const uint16_t toPos) {
    const uint16_t segSize = abs(segment.len);  //we want segment reference to be constant, but the size() function is not marked const in the library
    const uint16_t tgtSize = target.size();
    const uint16_t maxSize = tgtSize + segSize;
    if (fromPos == toPos || (fromPos >= maxSize && toPos >= maxSize))
        return true;
    const bool isOldEndSegmentWithinTarget = fromPos < tgtSize;
    const bool isNewEndSegmentWithinTarget = toPos < tgtSize;
    const bool isOldStartSegmentWithinTarget = fromPos >= segSize;
    const bool isNewStartSegmentWithinTarget = toPos >= segSize;
    const CRGB bkg = isOldEndSegmentWithinTarget ? target[fromPos] : target[fromPos - segSize - 1];   //if old pos (pixel after the segment end) wasn't within target boundaries, choose the pixel before the segment begin for background
    const uint16_t newPosTargetStart = qsuba(toPos, segSize);
    const uint16_t newPosTargetEnd = capu(toPos, target.size() - 1);
    if (toPos > fromPos) {
        if (isOldStartSegmentWithinTarget || isNewStartSegmentWithinTarget)
            target(qsuba(fromPos, segSize), qsuba(newPosTargetStart, 1)).nblend(bkg, overlay);
    } else {
        if (isOldEndSegmentWithinTarget || isNewEndSegmentWithinTarget)
            target(capu(fromPos, target.size()-1), qsuba(newPosTargetEnd, 1)).nblend(bkg, overlay);
    }
    const uint16_t startIndexSeg = isNewStartSegmentWithinTarget ? 0 : segSize - toPos;
    const uint16_t endIndexSeg = isNewEndSegmentWithinTarget ? segSize - 1 : maxSize - toPos - 1;
    CRGBSet sliceTarget((CRGB*)target, newPosTargetStart, newPosTargetEnd);
    const CRGBSet sliceSeg((CRGB*)segment, startIndexSeg, endIndexSeg);
    sliceTarget.nblend(sliceSeg, overlay);
    return fx::areSame(sliceTarget, sliceSeg);
}

/**
 * Are contents of the two sets the same - this is different than the == operator (that checks whether they point to the same thing)
 * @param lhs left-hand set
 * @param rhs right-hand set
 * @return true if the same length and same content
 */
bool fx::areSame(const CRGBSet &lhs, const CRGBSet &rhs) {
    if (abs(lhs.len) != abs(rhs.len))
        return false;
    for (uint16_t i = 0; i < static_cast<uint16_t>(abs(lhs.len)); ++i) {
        if (lhs[i] != rhs[i])
            return false;
    }
    return true;
}

/**
 * Replicate the source set into destination, repeating it as necessary to fill the entire destination
 * <p>Any overlaps between source and destination are skipped from replication - the source set backing array is guaranteed unchanged</p>
 * @param src source set
 * @param dest destination set
 */
void fx::replicateSet(const CRGBSet& src, CRGBSet& dest) {
    const uint16_t srcSize = abs(src.len);    //src.size() would be more appropriate, but the function is not marked const
    CRGB* normSrcStart = src.len < 0 ? src.end_pos : src.leds;     //src.reversed() would have been consistent, but function is not marked const
    CRGB* normSrcEnd = src.len < 0 ? src.leds : src.end_pos;
    CRGB* normDestStart = dest.reversed() ? dest.end_pos : dest.leds;
    CRGB* normDestEnd = dest.reversed() ? dest.leds : dest.end_pos;
    uint16_t x = 0;
    const bool hasOverlap = max(normSrcStart, normDestStart) < min(normSrcEnd, normDestEnd);
    for (auto &y : dest) {
        if (!hasOverlap || (&y < normSrcStart) || (&y >= normSrcEnd)) {
            y = src[x];
            incr(x, 1, srcSize);
        }
    }
}

/**
 * Replicate the source set into destination, repeating it with alternate mirroring as necessary to fill the entire destination
 * <p>Any overlaps between source and destination are skipped from replication - the source set backing array is guaranteed unchanged</p>
 * @param src source set
 * @param dest destination set
 * @param startMirrored whether the first replicated segment is a mirror of the source or a replica of the source
 */
void fx::replicateMirrorSet(const CRGBSet &src, CRGBSet &dest, const bool startMirrored) {
    const uint16_t srcSize = abs(src.len);    //src.size() would be more appropriate, but the function is not marked const
    CRGB* normSrcStart = src.len < 0 ? src.end_pos : src.leds;     //src.reversed() would have been consistent, but function is not marked const
    CRGB* normSrcEnd = src.len < 0 ? src.leds : src.end_pos;
    CRGB* normDestStart = dest.reversed() ? dest.end_pos : dest.leds;
    CRGB* normDestEnd = dest.reversed() ? dest.leds : dest.end_pos;
    const CRGBSet reversedSrc(src.leds, src.len-src.dir, 0);    // -src would have been consistent, but operator - is not marked const
    uint16_t x = 0;
    const bool hasOverlap = max(normSrcStart, normDestStart) < min(normSrcEnd, normDestEnd);
    bool mirror = startMirrored;  //start replicating with mirrored image - on the assumption that source is already part of the global output set
    for (auto &y : dest) {
        if (!hasOverlap || (&y < normSrcStart) || (&y >= normSrcEnd)) {
            const uint16_t lastX = x;
            y = mirror ? reversedSrc[x] : src[x];
            incr(x, 1, srcSize);
            if (x < lastX) mirror = !mirror;
        }
    }
}

/**
 * Replicate the source set into destination, repeating it with alternate mirroring as necessary to fill the entire destination
 * <p>Any overlaps between source and destination are skipped from replication - the source set backing array is guaranteed unchanged</p>
 * <p>Starts mirrored or not depending on whether the destination set is reversed</p>
 * @param src source set
 * @param dest destination set
 */
void fx::replicateMirrorSet(const CRGBSet &src, CRGBSet &dest) {
    replicateMirrorSet(src, dest, !dest.reversed());
}

/**
 * Populates and shuffles randomly an array of numbers in the range of 0-szArray (array indexes)
 * @param array array of indexes
 * @param szArray size of the array
 */
void fx::shuffleIndexes(uint16_t array[], uint16_t szArray) {
    //populates the indexes in ascending order
    for (uint16_t x = 0; x < szArray; x++)
        array[x] = x;
    //shuffle indexes
    const uint16_t swIter = (szArray >> 1) + (szArray >> 3);
    for (uint16_t x = 0; x < swIter; x++) {
        const uint16_t r = random16(0, szArray);
        const uint16_t tmp = array[x];
        array[x] = array[r];
        array[r] = tmp;
    }
}

void fx::shuffle(CRGBSet &set) {
    //perform a number of swaps with random elements of the array - randomness provided by ECC608 secure random number generator
    const uint16_t swIter = (set.size() >> 1) + (set.size() >> 4);
    for (uint16_t x = 0; x < swIter; x++) {
        const uint16_t r = random16(0, set.size());
        const CRGB tmp = set[x];
        set[x] = set[r];
        set[r] = tmp;
    }
}

/**
 * @brief Copies a subset of a CRGBSet into another CRGBSet, starting from index 0
 *
 * @param src source set
 * @param srcOfs source offset
 * @param dest destination set
 * @param destOfs destination offset
 * @param length number of elements to copy - a length of 0 or 1 has the same effect, copies one single pixel.
 */
void fx::copySet(const CRGBSet *src, CRGBSet *dest, const uint16_t length) {
    if (length >= abs(src->len) || length >= dest->size()) return;
    //memcpy(dest->leds, src->leds, sizeof(src->leds[0]) * length);
    const uint16_t len = length > 0 ? length - 1 : 0;
    CRGBSet source = *src;
    (*dest) = source(0, len);
}

/**
 * @brief Copy sets using pointer loops - one of the faster ways. Note this does not fill the destination set outside destOfs and length range
 *
 * Use `fillSet` for that behavior.
 *
 * @param src source set
 * @param srcOfs source offset
 * @param dest destination set
 * @param destOfs destination offset
 * @param length number of elements to copy - a length of 0 or 1 has the same effect, copies one single pixel.
 */
void fx::copySubSet(const CRGBSet *src, const uint16_t srcOfs, CRGBSet *dest, const uint16_t destOfs, const uint16_t length) {
    if (srcOfs >= abs(src->len) || destOfs >= dest->size()) return;
    const uint16_t len = length > 0 ? length - 1 : 0;
    if (srcOfs + len >= abs(src->len) || destOfs + len >= dest->size()) return;

    CRGBSet source = *src;
    dest->operator()(destOfs, destOfs+len) = source(srcOfs, srcOfs+len);
}

uint16_t fx::countPixelsBrighter(const CRGBSet *set, const CRGB backg) {
    const uint8_t bkgLuma = backg.getLuma();
    uint16_t ledsOn = 0;
    for (auto p: *set)
        if (p.getLuma() > bkgLuma)
            ledsOn++;

    return ledsOn;
}

bool fx::isAnyLedOn(const CRGB *arr, const uint16_t szArray, const CRGB backg) {
    const uint8_t bkgLuma = backg.getLuma();
    for (uint x = 0; x < szArray; x++) {
        if (arr[x].getLuma() > bkgLuma)
            return true;
    }
    return false;
}

bool fx::isAnyLedOn(CRGBSet *set, const CRGB backg) {
    if (backg == CRGB::Black)
        return (*set);
    return isAnyLedOn(set->leds, set->size(), backg);
}

void fx::fillSet(const CRGBSet *src, CRGBSet *dest, const uint16_t destOfs) {
    size_t curFrameIndex = destOfs;
    while (curFrameIndex < dest->size()) {
        const size_t len = capu(curFrameIndex + abs(src->len), dest->size()) - curFrameIndex;
        copySubSet(src, 0, dest, curFrameIndex, len);
        curFrameIndex += abs(src->len);
    }
}

void fx::mirrorLow(CRGBSet &set) {
    const int swaps = set.size()/2;
    for (int x = 0; x < swaps; x++) {
        set[set.size() - x - 1] = set[x];
    }
}

void fx::mirrorHigh(CRGBSet &set) {
    const int swaps = set.size()/2;
    for (int x = 0; x < swaps; x++) {
        set[x] = set[set.size() - x - 1];
    }
}

/**
 * Adjust the brightness of the given color
 * <p>Code borrowed from <code>ColorFromPalette</code> function</p>
 * @param color color to change
 * @param bright brightness to apply
 * @return new color instance with desired brightness
 */
CRGB fx::adjustBrightness(const CRGB color, uint8_t bright) {
    //inspired from ColorFromPalette implementation of applying the brightness factor
    uint8_t r = color.r, g = color.g, b = color.b;
    if (bright != 255) {
        if (bright) {
            ++bright; // adjust for rounding
            // Now, since localBright is nonzero, we don't need the full scale8_video logic;
            // we can just to scale8 and then add one (unless scale8 fixed) to all nonzero inputs.
            if (r)
                r = scale8_LEAVING_R1_DIRTY(r, bright);
            if (g)
                g = scale8_LEAVING_R1_DIRTY( g, bright);
            if (b)
                b = scale8_LEAVING_R1_DIRTY( b, bright);
            cleanup_R1();
        } else
            r = g = b = 0;
    }
    return {r, g, b};
}

/**
 * Blend multiply 2 colors
 * @param blendRGB base color, which is also the target (the one receiving the result)
 * @param topRGB color to multiply with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void fx::blendMultiply(CRGB &blendRGB, const CRGB &topRGB) {
    blendRGB.r = bmul8(blendRGB.r, topRGB.r);
    blendRGB.g = bmul8(blendRGB.g, topRGB.g);
    blendRGB.b = bmul8(blendRGB.b, topRGB.b);
}

/**
 * Blend multiply 2 color sets
 * <p>Wherever either layer was brighter than black, the composite is darker; since each value is less than 1,
 * their product will be less than each initial value that was greater than zero</p>
 * @param blendLayer base color set, which is also the target set (the one receiving the result)
 * @param topLayer color set to multiply with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void fx::blendMultiply(CRGBSet &blendLayer, const CRGBSet &topLayer) {
    for (auto bt = blendLayer.begin(), tp=topLayer.begin(), btEnd = blendLayer.end(), tpEnd = topLayer.end(); bt != btEnd && tp != tpEnd; ++bt, ++tp)
        blendMultiply(*bt, *tp);
}

/**
 * Blend screen 2 colors
 * @param blendRGB base color, which is also the target (the one receiving the result)
 * @param topRGB color to screen with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void fx::blendScreen(CRGB &blendRGB, const CRGB &topRGB) {
    blendRGB.r = bscr8(blendRGB.r, topRGB.r);
    blendRGB.g = bscr8(blendRGB.g, topRGB.g);
    blendRGB.b = bscr8(blendRGB.b, topRGB.b);
}

/**
 * Blend screen 2 color sets
 * <p>The result is the opposite of Multiply: wherever either layer was darker than white, the composite is brighter. </p>
 * @param blendLayer base color set, which is also the target set (the one receiving the result)
 * @param topLayer color set to screen with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void fx::blendScreen(CRGBSet &blendLayer, const CRGBSet &topLayer) {
    for (auto bt = blendLayer.begin(), tp=topLayer.begin(), btEnd = blendLayer.end(), tpEnd = topLayer.end(); bt != btEnd && tp != tpEnd; ++bt, ++tp)
        blendScreen(*bt, *tp);
}

/**
 * Blend overlay 2 colors
 * @param blendRGB base color, which is also the target (the one receiving the result)
 * @param topRGB color to overlay with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void fx::blendOverlay(CRGB &blendRGB, const CRGB &topRGB) {
    blendRGB.r = bovl8(blendRGB.r, topRGB.r);
    blendRGB.g = bovl8(blendRGB.g, topRGB.g);
    blendRGB.b = bovl8(blendRGB.b, topRGB.b);
}

/**
 * Blend overlay 2 color sets
 * <p>Where the base layer is light, the top layer becomes lighter; where the base layer is dark, the top becomes darker; where the base layer is mid-grey,
 * the top is unaffected. An overlay with the same picture looks like an S-curve. </p>
 * @param blendLayer base color set, which is also the target set (the one receiving the result)
 * @param topLayer color set to overlay with
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
void fx::blendOverlay(CRGBSet &blendLayer, const CRGBSet &topLayer) {
    for (auto bt = blendLayer.begin(), tp=topLayer.begin(), btEnd = blendLayer.end(), tpEnd = topLayer.end(); bt != btEnd && tp != tpEnd; ++bt, ++tp)
        blendOverlay(*bt, *tp);
}

/**
 * Repeatedly blends b into a with a given amount until a=b
 * @param a recipient
 * @param b source
 * @param amt amount of blending
 * @return true when the recipient is the same as source; false otherwise
 */
bool rblend8(uint8_t &a, const uint8_t b, const uint8_t amt) {
    if (a == b || amt == 0)
        return true;
    if (amt == 255) {
        a = b;
        return true;
    }
    if (a < b)
        a += scale8_video(b - a, amt);
    else
        a -= scale8_video(a - b, amt);
    return a == b;
}

/**
 * Blends the target color into an existing (in-place) such that after a number of iterations
 * the existing becomes equal with the target.
 * <p>For repeated blending calls, this works better than <code>nblend(CRGB &existing, CRGB &target, uint8 overlay)</code> as it
 * will actually make existing reach the target value</p>
 * @param existing color to modify
 * @param target target color
 * @param frOverlay fraction (number of 256-ths) of the target color to blend into existing. Special meaning to extreme values:
 * 0 is a no-op, existing is not changed; 255 forces the existing to equal to target
 * @return true if the existing color has become equal with target or overlay fraction is 0 (no blending); false otherwise
 */
bool fx::rblend(CRGB &existing, const CRGB &target, const fract8 frOverlay) {
    if (existing == target || frOverlay == 0)
        return true;
    if (frOverlay == 255) {
        existing = target;
        return true;
    }
    const bool bRed = rblend8(existing.red, target.red, frOverlay);
    const bool bGreen = rblend8(existing.green, target.green, frOverlay);
    const bool bBlue = rblend8(existing.blue, target.blue, frOverlay);
    return bRed && bGreen && bBlue;
}


/**
 * Adjust strip overall brightness according to the time of day - as follows:
 * <p>6am up until 10pm use the max brightness - i.e., <code>BRIGHTNESS</code></p>
 * <p>Between 10pm-11pm - reduce to 80% of full brightness, i.e., scale with 204</p>
 * <p>Between 11pm-12am - reduce to 70% of full brightness, i.e., scale with 178</p>
 * <p>After 12am - reduce to 60% of full brightness, i.e., scale with 152</p>
 */
uint8_t fx::adjustStripBrightness() {
    if (!stripBrightnessLocked && sysInfo->isSysStatus(SYS_STATUS_WIFI)) {
        const int hr = hour();

        // 6am–10pm: 0, 10pm–11pm: 204, 11pm–12am: 152, 12am–6am: 100
        const fract8 scale = (hr >= 6 && hr < 22) ? 0 :
                (hr >= 22 && hr < 23) ? 204 :
                (hr == 23)            ? 178 : 152;
        
        if (scale > 0)
            return dim8_raw(scale8(FastLED.getBrightness(), scale));
    }
    return FastLED.getBrightness();
}

/**
 *
 * @param rgb
 * @param br
 * @return
 */
CRGB& fx::setBrightness(CRGB &rgb, const uint8_t br) {
    CHSV hsv = fx::toHSV(rgb);
    hsv.val = br;
    rgb = fx::toRGB(hsv);
    return rgb;
}

/**
 * Approximate brightness - leverages <code>rgb2hsv_approximate</code>
 * @param rgb color
 * @return approximate brightness on HSV scale
 */
uint8_t fx::getBrightness(const CRGB &rgb) {
    //compare with rgb.getLuma() ?
    return fx::toHSV(rgb).val;
}

void adjustCurrentEffect(const time_t time) {
    fxRegistry.setSleepState(fxRegistry.isSleepEnabled() && !isAwakeTime(time));
}
