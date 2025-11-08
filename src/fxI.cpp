//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved
//
/**
 * Category I of light effects
 *
 */
#include "fxI.h"
#include "filesystem.h"
#include <algorithm>

using namespace FxI;
using namespace colTheme;

//~ Effect description strings stored in flash
constexpr auto fxi1Desc PROGMEM = "FXI1: Ping Pong";
constexpr auto fxi2Desc PROGMEM = "FXI2: Pacifica - gentle ocean waves";
constexpr auto fxi3Desc PROGMEM = "FXI3: Bouncy Ball";
constexpr auto fxi4Desc PROGMEM = "FXI4: Audio-seeded VU meter";

/**
 * Register FxI effects
 */
void FxI::fxRegister() {
    new FxI1();
    new FxI2();
    new FxI3();
    new FxI4();
}

//FXI1

FxI1::FxI1() : LedEffect(fxi1Desc) {
    wallStart = 0;
    wallEnd = FRAME_SIZE;
    prevWallStart = wallStart;
    prevWallEnd = wallEnd;
    currentPos = 0;
    midPointOfs = 0;
    segmentLength = 5;
    forward = true;
    fgColor = 0;
    bgColor = 0;
}

void FxI1::setup() {
    LedEffect::setup();
    bgColor = random8();
    fgColor = random8();
    forward = true;
    wallStart = random8(1, FRAME_SIZE / 4);
    wallEnd = FRAME_SIZE - wallStart;
    prevWallStart = 0;
    prevWallEnd = FRAME_SIZE;
    currentPos = wallStart;
    midPointOfs = 0;
    segmentLength = random8(1, 11);
    fgColor = random8();
    bgColor = -random8();
    tpl(0, wallStart - 1) = ColorFromPalette(targetPalette, bgColor, 7, LINEARBLEND);
    tpl(wallStart, prevWallEnd - 1) = BKG;
}

/**
 * Adjust walls at the end of a run
 */
void FxI1::reWall() {
    segmentLength = random8(1, 11);
    const uint8_t clrVar = random8(3, 19);
    bgColor += clrVar;
    fgColor += clrVar;
    if (forward) {
        wallEnd = FRAME_SIZE - random8(1, FRAME_SIZE / 4);
    } else {
        wallStart = random8(1, FRAME_SIZE / 4);
    }
    midPointOfs = random8(0, (wallEnd - wallStart) / 4) - (wallEnd - wallStart) / 8;
}

static void updateWall(uint16_t &prevWall, const uint16_t wallLimit, const CRGB color, const CRGB bg) {
    if (prevWall >= tpl.size() || wallLimit >= tpl.size()) {
        log_warn(F("UpdateWall parameters out of bounds for tpl size %d: prevWall=%d, wallLimit=%d. No changes made."), tpl.size(), prevWall, wallLimit);
        return;
    }
    if (prevWall != wallLimit) {
        if (prevWall > wallLimit)
            tpl[prevWall--] = color;
        else
            tpl[prevWall++] = bg;
    }
}

static void blendWall(const uint16_t start, const uint16_t end, const CRGB color) {
    if (tpl[end] != color)
        tpl(start, end).nblend(color, 80);
}

void FxI1::run() {
    EVERY_N_MILLISECONDS_I(speed, 75) {
        //update the wall sizes and color
        const CRGB wallColor = ColorFromPalette(targetPalette, bgColor, 7, LINEARBLEND);
        if (forward) {
            updateWall(prevWallEnd, wallEnd, wallColor, BKG);
            if (prevWallEnd == wallEnd)
                blendWall(wallEnd, FRAME_SIZE - 1, wallColor);
        } else {
            updateWall(prevWallStart, wallStart, BKG, wallColor);
            if (prevWallStart == wallStart)
                blendWall(0, wallStart - 1, wallColor);
        }

        // Fade the LED trail with a small dimming effect
        for (int i = wallStart; i < wallEnd; i++) {
            tpl[i].fadeToBlackBy(80);
        }

        // Draw the segment at the current position
        const CRGB clr = ColorFromPalette(palette, fgColor++, 255, LINEARBLEND);
        for (int i = 0; i < segmentLength; i++) {
            if ((currentPos + i) < wallEnd) {
                tpl[currentPos + i] = clr;
            }
        }

        // adjust the speed - increase in first half, decrease in second half
        const uint16_t midPoint = (wallEnd - wallStart) / 2;
        if (forward) {
            if (currentPos < midPoint + midPointOfs)
                speed = csub8(speed, 1, 20);
            else
                speed = cadd8(speed, 3, 250);
        } else {
            if (currentPos > midPoint - midPointOfs)
                speed = csub8(speed, 1, 20);
            else
                speed = cadd8(speed, 3, 250);
        }

        // Update the position of the segment
        if (forward) {
            currentPos++;
            if ((currentPos + segmentLength) >= wallEnd) {
                forward = false; // Reverse direction at the wall
                // speed = csub8(speed, 10, 20);
                reWall();
            }
        } else {
            currentPos--;
            if (currentPos <= wallStart) {
                forward = true; // Reverse direction at the wall
                //speed = random8(80, 160);
                reWall();
            }
        }

        // Display the updated LED state
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

uint8_t FxI1::selectionWeight() const {
    return 7;
}

//FXI2 - Pacifica gentle ocean waves
FxI2::FxI2(): LedEffect(fxi2Desc) {
}

void FxI2::setup() {
    LedEffect::setup();
    sCIStart1 = sCIStart2 = sCIStart3 = sCIStart4 = 0;
    sLastMs = 0;
}

// These three custom blue-green color palettes were inspired by the colors found in
// the waters off the southern coast of California, https: //goo.gl/maps/QQgd97jjHesHZVxQ7
static const CRGBPalette16 pacifica_palette_1 PROGMEM = {
    0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
    0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50
};
static const CRGBPalette16 pacifica_palette_2 PROGMEM = {
    0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
    0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F
};
static const CRGBPalette16 pacifica_palette_3 PROGMEM = {
    0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33,
    0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF
};


void FxI2::pacifica_loop() {
    // Increment the four "color index start" counters, one for each wave layer.
    // Each is incremented at a different speed, and the speeds vary over time.
    const uint32_t ms = millis();
    const uint32_t deltaMs = ms - sLastMs;
    sLastMs = ms;
    const uint16_t speedFactor1 = beatsin16(3, 179, 269);
    const uint16_t speedFactor2 = beatsin16(4, 179, 269);
    const uint32_t deltaMs1 = (deltaMs * speedFactor1) / 256;
    const uint32_t deltaMs2 = (deltaMs * speedFactor2) / 256;
    const uint32_t deltaMs21 = (deltaMs1 + deltaMs2) / 2;
    sCIStart1 += (deltaMs1 * beatsin88(1011, 10, 13));
    sCIStart2 -= (deltaMs21 * beatsin88(777, 8, 11));
    sCIStart3 -= (deltaMs1 * beatsin88(501, 5, 7));
    sCIStart4 -= (deltaMs2 * beatsin88(257, 4, 6));

    // Clear out the LED array to a dim background blue-green
    tpl.fill_solid(CRGB(2, 6, 10));

    // Render each of four layers, with different scales and speeds, that vary over time
    pacifica_one_layer(pacifica_palette_1, sCIStart1, beatsin16(3, 11 * 256, 14 * 256),
        beatsin8(10, 70, 130), 0 - beat16(301));
    pacifica_one_layer(pacifica_palette_2, sCIStart2, beatsin16(4, 6 * 256, 9 * 256),
        beatsin8(17, 40, 80), beat16(401));
    pacifica_one_layer(pacifica_palette_3, sCIStart3, 6 * 256, beatsin8(9, 10, 38), 0 - beat16(503));
    pacifica_one_layer(pacifica_palette_3, sCIStart4, 5 * 256, beatsin8(8, 10, 28), beat16(601));

    // Add brighter 'whitecaps' where the waves lines up more
    pacifica_add_whitecaps();

    // Deepen the blues and greens a bit
    pacifica_deepen_colors();
}

// Add one layer of waves into the LED array
void FxI2::pacifica_one_layer(const CRGBPalette16 &p, const uint16_t ciStart, const uint16_t waveScale, const uint8_t bri, const uint16_t ioff) {
    uint16_t ci = ciStart;
    uint16_t waveAngle = ioff;
    const uint16_t waveScale_half = (waveScale / 2) + 20;
    for (uint16_t i = 0; i < tpl.size(); i++) {
        waveAngle += 250;
        const uint16_t s16 = sin16(waveAngle) + 32768;
        const uint16_t cs = scale16(s16, waveScale_half) + waveScale_half;
        ci += cs;
        const uint16_t sIndex16 = sin16(ci) + 32768;
        const uint8_t sIndex8 = scale16(sIndex16, 240);
        const CRGB c = ColorFromPalette(p, sIndex8, bri, LINEARBLEND);
        tpl[i] += c;
    }
}

// Add extra 'white' to areas where the four layers of light have lined up brightly
void FxI2::pacifica_add_whitecaps() {
    const uint8_t baseThreshold = beatsin8(9, 55, 65);
    uint8_t wave = beat8(7);

    for (uint16_t i = 0; i < tpl.size(); i++) {
        const uint8_t threshold = scale8(sin8(wave), 20) + baseThreshold;
        wave += 7;
        if (const uint8_t l = tpl[i].getAverageLight(); l > threshold) {
            const uint8_t overage = l - threshold;
            const uint8_t overage2 = qadd8(overage, overage);
            tpl[i] += CRGB(overage, overage2, qadd8(overage2, overage2));
        }
    }
}

// Deepen the blues and greens
void FxI2::pacifica_deepen_colors() {
    for (uint16_t i = 0; i < tpl.size(); i++) {
        tpl[i].blue = scale8(tpl[i].blue, 145);
        tpl[i].green = scale8(tpl[i].green, 200);
        tpl[i] |= CRGB(2, 5, 7);
    }
}

/**
 * The code for this animation is more complicated than other examples, and while it is "ready to run",
 * and documented in general, it is probably not the best starting point for learning.  Nevertheless, it
 * does illustrate some useful techniques.
 * In this animation, there are four "layers" of waves of light.
 * Each layer moves independently, and each is scaled separately. All four wave layers are added together
 * on top of each other, and then another filter is applied that adds "whitecaps" of brightness where the
 * waves line up with each other more.  Finally, another pass is taken over the LED array to 'deepen' (dim)
 * the blues and greens.
 * The speed and scale and motion each layer varies slowly within independent hand-chosen ranges, which is
 * why the code has a lot of low-speed 'beatsin8' functions with a lot of oddly specific numeric ranges.
 *
 * Adapted from: https://gist.github.com/kriegsman/36a1e277f5b4084258d9af1eae29bac4
 * Name: Pacifica: gentle, blue-green ocean waves. For Dan.
 * Author: December 2019, Mark Kriegsman and Mary Corey March.
 */
void FxI2::run() {
    EVERY_N_MILLISECONDS_I(speed, 30) {
        pacifica_loop();
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

uint8_t FxI2::selectionWeight() const {
    return 9;
}

//FXI3 - Bouncy Ball
FxI3::FxI3() : LedEffect(fxi3Desc) {}

void FxI3::setup() {
    LedEffect::setup();
    // Soft background from target palette to make colors pop
    const uint8_t bgIdx = random8();
    const CRGB bg = ColorFromPalette(targetPalette, bgIdx, 6, LINEARBLEND);
    tpl.fill_solid(bg);

    // Physics init
    const uint16_t maxIdx = tpl.size() > 0 ? tpl.size() - 1 : 0;
    const int32_t maxPos = static_cast<int32_t>(maxIdx) * 256;
    pos256 = (random16(maxIdx + 1) * 256);
    dirRight = random8() & 0x01;
    vel256 = dirRight ? (random16(60, 220)) : -(int32_t)random16(60, 220); // initial speed
    gravity = random8(12, 40);            // pull per tick
    loss = random8(180, 230);             // 70%..90% energy retained on bounce
    trail = random8(3, 7);                // glow radius
    fadeAmt = random8(40, 80);            // trail fade amount
    hueIdx = random8();
    sparkTicks = 0;
}

void FxI3::run() {
    EVERY_N_MILLISECONDS_I(speed, 18) {
        // Fade and slight blur for glow trail
        tpl.fadeToBlackBy(fadeAmt);
        tpl.blur1d(48);

        // Physics update
        vel256 += (dirRight ? gravity : -gravity);
        pos256 += vel256;

        const uint16_t size = tpl.size();
        if (size == 0) return;
        const int32_t maxPos = static_cast<int32_t>(size - 1) * 256;

        bool bounced = false;
        if (pos256 < 0) {
            pos256 = 0;
            vel256 = -((int32_t)scale8((uint32_t)(vel256 >= 0 ? vel256 : -vel256), loss));
            dirRight = true;
            bounced = true;
        } else if (pos256 > maxPos) {
            pos256 = maxPos;
            vel256 = (int32_t)scale8((uint32_t)(vel256 >= 0 ? vel256 : -vel256), loss);
            vel256 = -vel256;
            dirRight = false;
            bounced = true;
        }

        if (bounced) {
            sparkTicks = 3;
            hueIdx += random8(12, 28); // shift color on bounce
        }

        // Draw bright core with glow using palette
        const uint16_t center = static_cast<uint16_t>(pos256 >> 8);
        const uint8_t frac = static_cast<uint8_t>(pos256 & 0xFF);
        const CRGB core = ColorFromPalette(palette, hueIdx, 255, LINEARBLEND);

        // Cross-fade between two adjacent pixels based on fractional position
        if (center < size) {
            const uint8_t briL = 255 - frac;
            const uint8_t briR = frac;
            CRGB leftPix = core; leftPix.nscale8_video(briL);
            tpl[center] += leftPix;
            if (center + 1 < size) { CRGB rightPix = core; rightPix.nscale8_video(briR); tpl[center + 1] += rightPix; }
        }

        // Radial trail falloff
        const uint8_t baseGlow = 160;
        for (uint8_t r = 1; r <= trail; ++r) {
            const uint8_t glowBri = scale8(baseGlow, qsub8(255, r * (255 / (trail + 1))));
            const CRGB glow = ColorFromPalette(palette, hueIdx + r * 6, glowBri, LINEARBLEND);
            const int32_t li = (int32_t)center - r;
            const int32_t ri = (int32_t)center + r + 1; // account for subpixel leaning to the right
            if (li >= 0 && (uint16_t)li < size) tpl[(uint16_t)li] += glow;
            if (ri >= 0 && (uint16_t)ri < size) tpl[(uint16_t)ri] += glow;
        }

        // Brief spark/flash on bounce for eye-catching pop
        if (sparkTicks > 0) {
            sparkTicks--;
            const uint8_t flashBri = 200;
            const CRGB flash = CHSV(hueIdx, 40, 255) + CRGB(flashBri, flashBri, flashBri);
            const uint16_t c = center;
            if (c < size) tpl[c] = tpl[c] + flash;
            if (c > 0) tpl[c - 1] += flash;
            if (c + 1 < size) tpl[c + 1] += flash;
        }

        hueIdx += 2; // gentle hue drift

        // Output
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

uint8_t FxI3::selectionWeight() const {
    return 10;
}

// FXI4 - Audio-seeded VU meter
FxI4::FxI4() : LedEffect(fxi4Desc) {}

void FxI4::loadSeedFromFile() {
    seed.clear();
    String content;
    if (const size_t sz = SyncFsImpl.readFile(seedFile, &content); sz == 0 || content.length() == 0) {
        log_warn(F("FxI4: seed file '%s' not found or empty. Using pseudo-random seed."), seedFile);
        // Fill with a pseudo-random envelope so the effect still works
        seed.reserve(1024);
        uint8_t val = random8();
        for (int i = 0; i < 1024; ++i) { val = qadd8(scale8(val, 200), random8(55)); seed.push_back(val); }
        // pseudo-random mono envelope
        seedHasBands = false;
        frames = 0;
        seedPos = 0;
        framePos = 0;
        return;
    }

    // Parse numbers 0..255 separated by non-digit characters
    uint16_t acc = 0;
    bool inNum = false;
    for (size_t i = 0; i < content.length(); ++i) {
        char c = content.charAt(i);
        if (c >= '0' && c <= '9') {
            acc = (uint16_t)acc * 10u + (uint16_t)(c - '0');
            inNum = true;
        } else {
            if (inNum) {
                seed.push_back((uint8_t)min<uint16_t>(acc, 255));
                acc = 0; inNum = false;
            }
        }
    }
    if (inNum) seed.push_back((uint8_t)min<uint16_t>(acc, 255));

    if (seed.empty()) {
        log_warn(F("FxI4: seed parse yielded no values. Falling back to noise."));
        seed.reserve(512);
        for (int i = 0; i < 512; ++i) seed.push_back(random8());
    }

    // Determine if seed provides per-band frames: must be divisible by current segment count
    if (segments > 0 && (seed.size() % segments) == 0) {
        seedHasBands = true;
        frames = seed.size() / segments;
    } else {
        if (segments > 0)
            log_warn(F("FxI4: seed size %u not divisible by segments %u. Using mono envelope mode."), (unsigned)seed.size(), segments);
        seedHasBands = false;
        frames = 0;
    }
    seedPos = 0;
    framePos = 0;
}

uint8_t FxI4::levelFromSeed(const uint8_t band) {
    if (seed.empty()) return random8();
    if (seedHasBands && segments > 0) {
        // Row-major: frame0_band0..bandN-1, frame1_...
        const size_t idx = (framePos % max<size_t>(1, frames)) * segments + (band % segments);
        return seed[idx % seed.size()];
    }
    // Legacy mono envelope: advance position per sample
    const uint8_t v = seed[seedPos++];
    if (seedPos >= seed.size()) seedPos = 0;
    return v;
}

void FxI4::setup() {
    LedEffect::setup();
    baseHue = random8();

    // Decide segment count based on strip size if needed
    const uint16_t n = tpl.size();
    if (n >= 90) segments = 12;
    else if (n >= 60) segments = 10;
    else if (n >= 40) segments = 8;
    else if (n >= 20) segments = 6;
    else segments = 4;

    hist.assign(segments, 0);
    peaks.assign(segments, 0);
    peakTs.assign(segments, 0);

    loadSeedFromFile();

    tpl.fill_solid(BKG);
}

void FxI4::drawSegments() {
    const uint16_t total = tpl.size();
    if (total == 0) return;

    const uint16_t gapsTotal = (segments > 0 ? (segments - 1) : 0) * segmentGap;
    const uint16_t usable = total > gapsTotal ? (total - gapsTotal) : total;
    const uint16_t segWidth = segments ? max<uint16_t>(1, usable / segments) : usable;

    // Background fade for trailing effect
    tpl.fadeToBlackBy(40);

    uint16_t x = 0;
    for (uint8_t s = 0; s < segments; ++s) {
        const uint16_t segStart = x;
        const uint16_t segEnd = min<uint16_t>(segStart + segWidth, total);
        const uint16_t height = segEnd - segStart;

        // Compute smoothed level per segment by sampling next seed value and applying IIR
        const uint8_t raw = levelFromSeed(s);
        const uint8_t prev = hist[s];
        const uint16_t sm = ((uint16_t)prev * (uint16_t)(255 - smoothing) + (uint16_t)raw * (uint16_t)smoothing) / 255u;
        const uint8_t lvl = (uint8_t)sm;
        hist[s] = lvl;

        // Peak hold logic per segment
        if (lvl >= peaks[s]) { peaks[s] = lvl; peakTs[s] = millis(); }
        else if (millis() - peakTs[s] > peakHoldMs) { peaks[s] = qsub8(peaks[s], 2); }

        // Map level 0..255 to number of lit pixels in this segment
        const uint16_t lit = (uint32_t)lvl * height / 255u;
        const uint16_t peakPix = (uint32_t)peaks[s] * height / 255u;

        // Choose color per segment and gradient by height using palette
        const uint8_t segHue = baseHue + s * (240 / max<uint8_t>(1, segments));

        // Draw from bottom (segStart) upwards
        for (uint16_t i = 0; i < height; ++i) {
            const uint16_t idx = segStart + i;
            if (idx >= total) break;
            if (i < lit) {
                // Brightness increases toward the top; color varies slightly with i
                const uint8_t bri = scale8(40 + (i * 215) / max<uint16_t>(1, height - 1), stripBrightness);
                const CRGB col = ColorFromPalette(palette, segHue + i * 3, bri, LINEARBLEND);
                tpl[idx] = col;
            } else {
                // base background, slightly tinted by palette
                const CRGB col = ColorFromPalette(palette, segHue, 4, LINEARBLEND);
                tpl[idx] = col;
            }
        }
        // Peak marker: a bright thin line
        if (peakPix < height) {
            const uint16_t pidx = segStart + peakPix;
            if (pidx < total) {
                CRGB peakColor = ColorFromPalette(palette, segHue + 16, 255, LINEARBLEND);
                tpl[pidx] = peakColor;
            }
        }

        x = segEnd + segmentGap; // advance with gap
    }

    // Advance to next frame if seed provides per-band frames
    if (seedHasBands && frames > 0) {
        framePos = (framePos + 1) % frames;
    }

    baseHue += 1; // slow drift across palette
}

void FxI4::run() {
    EVERY_N_MILLISECONDS_I(speed, frameMs) {
        drawSegments();
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

uint8_t FxI4::selectionWeight() const { return 12; }
