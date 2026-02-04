//
// Copyright (c) 2023,2024,2025,2026 by Dan Luca. All rights reserved
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
static const EffectInfo fxi1Desc PROGMEM = {EFFECT_FACTORY(FxI1), "FXI1", "Ping Pong", 7};
static const EffectInfo fxi2Desc PROGMEM = {EFFECT_FACTORY(FxI2), "FXI2", "Pacifica - gentle ocean waves", 9};
static const EffectInfo fxi3Desc PROGMEM = {EFFECT_FACTORY(FxI3), "FXI3", "Bouncy Ball", 10};
static const EffectInfo fxi4Desc PROGMEM = {EFFECT_FACTORY(FxI4), "FXI4", "Audio-seeded VU meter", 12};
static const EffectInfo fxi5Desc PROGMEM = {EFFECT_FACTORY(FxI5), "FXI5", "Shore waves with backwash", 10};
static const EffectInfo fxi6Desc PROGMEM = {EFFECT_FACTORY(FxI6), "FXI6", "Bowling alley", 9};

/**
 * Register FxI effects
 */
void FxI::fxRegister() {
    fxRegistry.registerEffect(&fxi1Desc);
    fxRegistry.registerEffect(&fxi2Desc);
    fxRegistry.registerEffect(&fxi3Desc);
    fxRegistry.registerEffect(&fxi4Desc);
    fxRegistry.registerEffect(&fxi5Desc);
    fxRegistry.registerEffect(&fxi6Desc);
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
    if (const bool pwExceeds = prevWall >= tpl.size(); pwExceeds || wallLimit >= tpl.size()) {
        log_warn(F("UpdateWall parameters out of bounds for tpl size %d: prevWall=%d, wallLimit=%d. No changes made."), tpl.size(), prevWall, wallLimit);
        if (pwExceeds)
            prevWall--;
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
    if (start >= tpl.size() || end >= tpl.size()) return;
    if (tpl[end] != color)
        tpl(start, end).nblend(color, 64);
}

void FxI1::run() {
    EVERY_N_MILLISECONDS_I(speed, 75) {
        //update the wall sizes and color
        const CRGB wallColor = ColorFromPalette(targetPalette, bgColor, 7, LINEARBLEND);
        if (forward) {
            if (prevWallEnd == wallEnd)
                blendWall(wallEnd, FRAME_SIZE - 1, wallColor);
            else
                updateWall(prevWallEnd, wallEnd, wallColor, BKG);
        } else {
            if (prevWallStart == wallStart)
                blendWall(0, wallStart - 1, wallColor);
            else
                updateWall(prevWallStart, wallStart, BKG, wallColor);
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

//FXI3 - Bouncy Ball (vertical drop with damped bounces)
FxI3::FxI3() : LedEffect(fxi3Desc) {}

void FxI3::setup() {
    LedEffect::setup();
    // Soft background from target palette to make colors pop
    const uint8_t bgIdx = random8();
    const CRGB bg = ColorFromPalette(targetPalette, bgIdx, 6, LINEARBLEND);
    tpl.fill_solid(bg);

    // Physics init for vertical drop: start near the top, fall toward floor at index 0
    const uint16_t size = tpl.size();
    const uint16_t maxIdx = size > 0 ? static_cast<uint16_t>(size - 1) : 0;
    const int32_t maxPos = static_cast<int32_t>(maxIdx) * 256;
    init_drop(maxPos);
    hueIdx = random8();
    restHold = 0;
}

void FxI3::init_drop(const int32_t maxPos) {
    // restart a new drop with a (possibly) new direction
    dirRight = random8() & 1;
    pos256 = maxPos;
    vel256 = 0;
    gravity = -static_cast<int16_t>(random8(12, 26)); // downward pull per tick (toward index 0)
    // Target ~3/4 height on rebound: e ≈ sqrt(0.75) ≈ 0.866 → scale8 ≈ 221
    // Keep a narrowband around that so bounces feel natural but still high
    loss = random8(205, 230);   // velocity retained on bounce (~0.804..0.902)
    trail = random8(2, 5);      // glow radius
    fadeAmt = random8(40, 80);  // trail fade amount
    sparkTicks = 0;
    settled = false;
}

void FxI3::run() {
    EVERY_N_MILLISECONDS_I(speed, 18) {
        const uint16_t size = tpl.size();
        CRGBSet revTpl = -tpl;
        const int32_t maxPos = static_cast<int32_t>(size - 1) * 256;

        // Fade and slight blur for glow trail
        CRGBSet &frame = dirRight ? tpl : revTpl;
        frame.fadeToBlackBy(fadeAmt);
        frame.blur1d(24);

        // If settled, hold a dim resting ball at the floor then restart
        if (settled) {
            if (restHold > 0) restHold--;
            // draw a very dim dot on the floor (index 0)
            const CRGB restCol = ColorFromPalette(palette, hueIdx, 40, LINEARBLEND);
            frame[0] += restCol;

            if (restHold == 0) {
                // restart a new drop
                init_drop(maxPos);
                hueIdx += random8(10, 25);
                speed.setPeriod(random8(18, 48));
            }
        } else {
            // Physics update: fall towards the floor (index 0)
            vel256 += gravity;
            pos256 += vel256;

            bool bounced = false;
            // Floor collision
            if (pos256 <= 0) {
                pos256 = 0;
                // reflect velocity and apply energy loss
                int32_t vabs = vel256 >= 0 ? vel256 : -vel256;
                vabs = (static_cast<uint32_t>(vabs) >> 8) * loss;

                // Decide whether to settle: if the post-loss velocity is too small
                // raise the threshold a bit to avoid endless micro "ripples"
                if (constexpr int32_t kSettleThresh = 36; vabs < kSettleThresh) {
                    vel256 = 0;
                    settled = true;
                    restHold = random16(500 / 18, 1500 / 18); // ~0.5s..1.5s worth of frames at 18ms
                } else {
                    vel256 = vabs; // now upward (positive)
                    bounced = true;
                }
            }
            // Prevent going above the top by clamping at maxPos (no ceiling bounce)
            if (pos256 > maxPos) {
                pos256 = maxPos;
                if (vel256 > 0) vel256 = (vel256 >> 1); // damp if overshoot
            }

            if (bounced && !settled) {
                sparkTicks = 2;
                hueIdx += random8(8, 20); // slight color shift on bounce
            }

            // Draw bright core with glow using palette, brightness scales with speed
            const auto center = static_cast<uint16_t>(pos256 >> 8);
            const auto frac = static_cast<uint8_t>(pos256 & 0xFF);

            // speed-based brightness
            auto spd = static_cast<uint32_t>(vel256 >= 0 ? vel256 : -vel256);
            spd = min<uint32_t>(spd, 512u); // cap
            const uint8_t coreBri = qadd8(110, scale8((uint8_t)min<uint32_t>(255, spd), 160));
            const CRGB coreCol = ColorFromPalette(palette, hueIdx, coreBri, LINEARBLEND);

            if (center < size) {
                const uint8_t briL = 255 - frac;
                const uint8_t briR = frac;
                CRGB leftPix = coreCol; leftPix.nscale8_video(briL);
                frame[center] += leftPix;
                if (center + 1 < size) { CRGB rightPix = coreCol; rightPix.nscale8_video(briR); frame[center + 1] += rightPix; }
            }

            // Radial trail falloff
            constexpr uint8_t baseGlow = 150;
            for (uint8_t r = 1; r <= trail; ++r) {
                const uint8_t glowBri = scale8(baseGlow, qsub8(255, r * (255 / (trail + 1))));
                const CRGB glow = ColorFromPalette(palette, hueIdx + r * 6, glowBri, LINEARBLEND);
                const int32_t li = static_cast<int32_t>(center) - r;
                const int32_t ri = static_cast<int32_t>(center) + r + 1; // slight forward smear
                if (li >= 0 && static_cast<uint16_t>(li) < size) frame[static_cast<uint16_t>(li)] += glow;
                if (ri >= 0 && static_cast<uint16_t>(ri) < size) frame[static_cast<uint16_t>(ri)] += glow;
            }

            // Brief spark/flash on bounce for eye-catching pop
            if (sparkTicks > 0) {
                sparkTicks--;
                constexpr uint8_t flashBri = 180;
                const CRGB flash = CHSV(hueIdx, 40, 255) + CRGB(flashBri, flashBri, flashBri);
                const auto c = static_cast<uint16_t>(pos256 >> 8);
                if (c < size) frame[c] = frame[c] + flash;
                if (c > 0) frame[c - 1] += flash;
                if (c + 1 < size) frame[c + 1] += flash;
            }
        }

        // Gentle hue drift overall
        hueIdx += 1;

        // Output
        replicateMirrorSet(frame, others, dirRight);
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
    auto* content = new String();   //allocate on the heap, potentially large file size
    String seedFile(seedFiles_prefix);
    seedFile += (random8() % 4 + 1);
    seedFile += seedFile_extension;
    if (const size_t sz = SyncFsImpl.readFile(seedFile.c_str(), content); sz == 0 || content->length() == 0) {
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
        delete content;
        return;
    }

    // Parse numbers 0..255 separated by non-digit characters
    uint16_t acc = 0;
    bool inNum = false;
    for (size_t i = 0; i < content->length(); ++i) {
        if (const char c = content->charAt(i); c >= '0' && c <= '9') {
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
    delete content;
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

    // Decide segment count based on strip size if needed - 4 longer segments are better though
    segments = 4;

    hist.assign(segments, 0);
    peaks.assign(segments, 0);
    peakTs.assign(segments, 0);

    loadSeedFromFile();

    tpl.fill_solid(BKG);
}

void FxI4::cleanup() {
    // Free large buffers and shrink capacity to release heap
    seed.clear(); seed.shrink_to_fit();
    hist.clear(); hist.shrink_to_fit();
    peaks.clear(); peaks.shrink_to_fit();
    peakTs.clear(); peakTs.shrink_to_fit();
    seedPos = 0; framePos = 0; frames = 0; seedHasBands = false;
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
        speed.setPeriod(frameMs);
    }
}

uint8_t FxI4::selectionWeight() const { return 12; }

// FXI5 - Shoreline waves: approaching swell, shore whitecaps, and backwash
FxI5::FxI5() : LedEffect(fxi5Desc), frame(leds, frameSize), rest(leds, frameSize, NUM_PIXELS-1) {
}

void FxI5::setup() {
    LedEffect::setup();
    seaHueBase = random8();
    foamLevel = 0;
    backwashActive = false;
    const uint16_t n = frame.size();
    {
        uint16_t base = (n / 10u);
        if (base < 6) base = 6;
        if (base > 20) base = 20;
        swellWidth = base;
    }
    backwashWidth = max<uint16_t>(5, swellWidth - 3);
    swellPos = n ? (n - 1 + swellWidth) : 0; // start off-screen to the right
    backwashPos = 0;
    // Configure beach according to request 5-10 pixels, but never exceeding frame size
    beachLen = random8(kBeachMin, kBeachMax);
    beachWashDepth = min<uint8_t>(beachWashDepth, beachLen);
    // Reset wetness state
    for (unsigned char & i : beachWet) i = 0;
    ledSet.fill_solid(BKG);
}

void FxI5::cleanup() {
    // Reset all state variables to initial values
    swellPos = 0;
    swellCrashed = false;
    crashHold = 0;
    foamLevel = 0;
    lastTick = 0;
    backwashActive = false;
    backwashPos = 0;
    seaHueBase = 0;
    beachLen = kBeachMin;
    // Clear beach wetness array
    memset(beachWet, 0, sizeof(beachWet));
}

void FxI5::drawSeaBackground() {
    const uint16_t n = frame.size();
    if (!n) return;
    // Dim base using palette hues; darker near shore (index beachLen), slightly brighter offshore
    for (uint16_t i = 0; i < n; ++i) {
        // Initialize with background; beach area will be drawn in drawBeach()
        frame[i] = BKG;
    }
    if (beachLen >= n) return; // all frame is beach in extreme small strips
    for (uint16_t i = beachLen; i < n; ++i) {
        const uint8_t depth = scale8((uint8_t)((uint32_t)i * 255 / max<uint16_t>(1, n - 1)), 200);
        const uint8_t bri = 4 + scale8(depth, 24); // 4..28
        const uint8_t idx = seaHueBase + scale8(i, 3); // slow gradient along strip
        frame[i] = ColorFromPalette(targetPalette, idx, bri, LINEARBLEND);
    }
}

void FxI5::drawSwell(const uint16_t center, const uint16_t width, const uint8_t crestBri, const int8_t dirSign) {
    const uint16_t n = frame.size();
    if (width == 0) return;
    const int16_t half = width / 2;
    const auto c = (int16_t)center;
    // Draw a soft bell curve; add cool tint from palette and a whitecap at crest
    for (int16_t dx = -half; dx <= half; ++dx) {
        const int16_t pos = c + dx;
        // keep the moving water out of the sandy beach: never draw below beachLen
        if (pos < (int16_t)beachLen || pos >= (int16_t)n) continue;
        // Parabolic falloff 1 - (x/w)^2
        const int16_t adx = abs(dx);
        const uint8_t base = qsub8(255, scale8((uint8_t)((uint32_t)adx * 255 / max<int16_t>(1, half)), (uint8_t)((uint32_t)adx * 255 / max<int16_t>(1, half))));
        const uint8_t bri = scale8(base, crestBri);
        const uint8_t hueOfs = (dirSign < 0 ? 12 : 4);
        const CRGB sea = ColorFromPalette(targetPalette, seaHueBase + hueOfs + (uint8_t)(dx * 2), bri, LINEARBLEND);
        frame[(uint16_t)pos] += sea;
        // Whitecap within inner third
        if ((uint16_t)adx <= max<uint16_t>(1, width / 6)) {
            const uint8_t wcap = scale8(255 - (uint8_t)((uint32_t)adx * 255 / max<uint16_t>(1, width / 6)), crestBri);
            frame[(uint16_t)pos] += CRGB(wcap, wcap, wcap);
        }
    }
}

void FxI5::drawFoamAtShore(const uint8_t level) {
    if (level == 0) return;
    const uint16_t n = frame.size();
    const uint16_t span = min<uint16_t>(beachLen, max<uint16_t>(2, min<uint16_t>(n / 12, 16)));
    for (uint16_t i = 0; i < span; ++i) {
        const uint8_t atten = 255 - (uint8_t)((uint32_t)i * 255 / span);
        const uint8_t bri = scale8(level, atten);
        frame[beachLen - i -1] += CRGB(bri, bri, bri);
    }
}

void FxI5::drawBeach() {
    if (beachLen == 0) return;
    const uint16_t n = frame.size();
    const uint8_t len = min<uint16_t>(beachLen, n);
    const uint8_t idxDry = seaHueBase + beachHueDryOfs;
    const uint8_t idxWet = seaHueBase + beachHueWetOfs;
    for (uint8_t i = 0; i < len; ++i) {
        const uint8_t w = beachWet[i];
        const CRGB dryCol = ColorFromPalette(targetPalette, idxDry, beachBriDry, LINEARBLEND);
        const CRGB wetCol = ColorFromPalette(targetPalette, idxWet, beachBriWet, LINEARBLEND);
        const CRGB sand = blend(dryCol, wetCol, w);
        frame[i] = sand; // base beach before foam overlays
    }
}

void FxI5::drawSplash(const uint8_t intensity) {
    if (intensity == 0) return;
    const uint16_t n = frame.size();
    if (beachLen >= n) return;
    // spray into the first few water pixels and a touch on the last beach pixel
    const uint16_t seaSpan = max<uint16_t>(2, min<uint16_t>(n / 16, 8));
    const uint8_t beachTouch = beachLen ? (uint8_t)1 : (uint8_t)0;

    // brighten last beach pixel to mimic splash wetting
    if (beachTouch) {
        uint8_t bri = scale8(intensity, 200);
        frame[beachLen - 1] += CRGB(bri, bri, bri);
    }
    // sea side spray with quick fade out
    for (uint16_t i = 0; i < seaSpan && (beachLen + i) < n; ++i) {
        const uint8_t atten = 255 - (uint8_t)((uint32_t)i * 255 / seaSpan);
        const uint8_t bri = scale8(intensity, atten);
        frame[beachLen + i] += CRGB(bri, bri, bri);
    }
}

void FxI5::updateBeachWetness(const bool impactNow) {
    if (beachLen == 0) return;
    const uint8_t len = beachLen;
    // natural drying
    for (uint8_t i = 0; i < len; ++i) {
        beachWet[i] = qsub8(beachWet[i], beachDryRate);
    }
    // wave impact wets front of beach with a gradient
    if (impactNow) {
        const uint8_t wash = min<uint8_t>(beachWashDepth, len);
        for (uint8_t i = 0; i < wash; ++i) {
            const uint8_t atten = 255 - (uint8_t)((uint16_t)i * 255 / max<uint8_t>(1, wash));
            const uint8_t boost = scale8(beachWetBoost, atten);
            beachWet[i] = qadd8(beachWet[i], boost);
        }
    }
    // gentle re-wet from backwash touching first pixel or two while active
    if (backwashActive && len) {
        const uint8_t touch = min<uint8_t>(2, len);
        for (uint8_t i = 0; i < touch; ++i) {
            beachWet[i] = qadd8(beachWet[i], 12);
        }
    }
}

void FxI5::run() {
    EVERY_N_MILLISECONDS_I(tmr, 40) {
        const uint16_t n = frame.size();

        // Base sea each frame
        drawSeaBackground();

        // Move swell towards shore until it reaches the waterline (left edge of sea at index beachLen)
        const uint16_t crestHalf = (swellWidth / 2);
        const uint16_t waterline = beachLen; // first sea pixel
        if (!swellCrashed) {
            if (swellPos > 0) {
                // compute leading edge position
                uint16_t leadingEdge = (swellPos > crestHalf) ? (uint16_t)(swellPos - crestHalf) : 0;
                if (leadingEdge > waterline) {
                    // advance normally
                    swellPos -= 1;
                } else {
                    // reached the shore: crash!
                    swellCrashed = true;
                    crashHold = 6 + random8(0, 10); // hold crest briefly
                    foamLevel = qadd8(foamLevel, 160);
                    // Do NOT start backwash immediately; wait until the crash hold finishes
                }
            }
        } else {
            // keep crest right at the shoreline during crash
            swellPos = waterline + crestHalf;
            if (crashHold > 0) crashHold--; else {
                // Start backwash only after the crest hold completes, creating a brief pause
                if (!backwashActive) {
                    backwashActive = true;
                    backwashPos = waterline + min<uint16_t>(crestHalf + 1, n / 8);
                    lastTick = millis();
                }
                swellCrashed = false;
                // drop the swell so we can respawn
                swellPos = 0;
            }
        }

        // Consider impact state for beach wetness and splash this frame
        const bool impact = swellCrashed;

        // Update beach wetness and draw beach after base sea
        updateBeachWetness(impact);
        drawBeach();

        // Draw approaching swell (if still on strip or slightly off to right)
        if (swellPos < n + swellWidth && !swellCrashed) {
            const uint16_t center = swellPos;
            drawSwell(center, swellWidth, 160, -1);
        }
        // Draw the breaking crest frozen at shoreline while crashing
        if (swellCrashed) {
            drawSwell(waterline + crestHalf, swellWidth, 200, -1);
            drawSplash(foamLevel);
        }

        // Backwash recedes from shore briefly
        if (backwashActive) {
            drawSwell(backwashPos, backwashWidth, 80, +1);
            // advance and fade out
            if (millis() - lastTick > backwashPeriodMs) {
                lastTick = millis();
                backwashPos = static_cast<uint16_t>(backwashPos + 1);
                if (backwashPos >= waterline + min<uint16_t>(n / 3, static_cast<uint16_t>(swellWidth * 3))) {
                    backwashActive = false;
                }
            }
        }

        // Shore foam fade (overlays beach and sea)
        drawFoamAtShore(foamLevel);
        foamLevel = qsub8(foamLevel, 18);

        // Re-spawn swell after it fully passed the shore
        if (swellPos == 0 && !swellCrashed) {
            // Start a new swell from offshore with slight randomness
            {
                auto base = (uint16_t)(n / 10u + random8(0, 6));
                if (base < 6) base = 6;
                if (base > 24) base = 24;
                swellWidth = base;
            }
            swellPos = n - 1 + swellWidth + random8(6, 20);
            seaHueBase += random8(3, 9); // slow color drift
            // constant frame rate, or opportunity to modify it
            tmr.setPeriod(30 + random8(0, 30));
            backwashPeriodMs = tmr.getPeriod() * 10/3;
        }
        replicateMirrorSet(frame, rest);
        FastLED.show(stripBrightness);
    }
}

uint8_t FxI5::selectionWeight() const { return 10; }

// FXI6 - Bowling alley simulation
FxI6::FxI6() : LedEffect(fxi6Desc) {}

void FxI6::setup() {
    LedEffect::setup();
    laneStart = 0;
    laneEnd = tpl.size() > 6 ? tpl.size() - 1 : tpl.size(); // guard
    // choose colors from current palette
    ballHue = random8();
    pinHue = ballHue + random8(40, 100);
    resetFrame(false);
}

void FxI6::layoutPins() {
    // Place 10 pins toward the far end, spaced evenly, with slight jitter if space allows
    const uint16_t laneLen = laneEnd - laneStart + 1;
    uint16_t zoneStart = laneStart + (laneLen * 2) / 3;
    if (zoneStart + kPins >= laneEnd) {
        zoneStart = laneEnd > (kPins + 1) ? laneEnd - (kPins + 1) : laneStart;
    }
    const uint16_t zoneLen = laneEnd - zoneStart + 1;
    const uint16_t step = zoneLen > kPins ? max<uint16_t>(1, zoneLen / kPins) : 1;
    uint16_t pos = zoneStart;
    for (uint8_t i = 0; i < kPins; i++) {
        uint16_t p = pos;
        if (zoneLen >= kPins * 2) {
            // small jitter within [-1, +1]
            const int8_t j = (int8_t)random8(0, 3) - 1;
            int32_t pj = (int32_t)p + j;
            if (pj < (int32_t)zoneStart) pj = zoneStart;
            if (pj > (int32_t)laneEnd) pj = laneEnd;
            p = (uint16_t)pj;
        }
        pinPos[i] = (uint8_t)constrain(p, laneStart, laneEnd);
        pos = (uint16_t)min<uint32_t>(laneEnd, (uint32_t)pos + step);
    }
}

void FxI6::resetFrame(bool randomizeColors) {
    if (randomizeColors) {
        ballHue += random8(10, 40);
        pinHue = ballHue + random8(40, 100);
    }
    // clear state
    for (uint8_t i = 0; i < kPins; i++) {
        pinUp[i] = true;
        pinSpark[i] = 0;
    }
    layoutPins();
    ballPos = laneStart;
    ballVel = random8(0, 100) < 60 ? 1 : 2; // mostly 1, sometimes 2
    lastReset = millis();
}

void FxI6::run() {
    EVERY_N_MILLISECONDS_I(tmr, frameMs) {
        // Background lane color (dim, from palette)
        const CRGB laneClr = ColorFromPalette(targetPalette, pinHue, 10, LINEARBLEND);
        tpl.fill_solid(laneClr);

        // Draw pins
        for (uint8_t i = 0; i < kPins; i++) {
            const uint8_t p = pinPos[i];
            if (pinUp[i]) {
                // standing pin: brighter color
                tpl[p] = ColorFromPalette(targetPalette, pinHue, 200, LINEARBLEND);
                // subtle highlight
                if (p > laneStart) nblend(tpl[p - 1], ColorFromPalette(targetPalette, pinHue + 8, 120), 100);
                if (p < laneEnd) nblend(tpl[p + 1], ColorFromPalette(targetPalette, pinHue + 8, 120), 100);
            } else if (pinSpark[i] > 0) {
                // knocked: sparkle and fade out
                const uint8_t bri = pinSpark[i];
                tpl[p] += ColorFromPalette(palette, pinHue + 32, bri, LINEARBLEND);
                if (p > laneStart) tpl[p - 1] += ColorFromPalette(palette, pinHue + 48, bri / 2, LINEARBLEND);
                if (p < laneEnd) tpl[p + 1] += ColorFromPalette(palette, pinHue + 48, bri / 2, LINEARBLEND);
                pinSpark[i] = qsub8(pinSpark[i], 18);
            }
        }

        // Draw ball with small trail
        const CRGB ballClr = ColorFromPalette(palette, ballHue, 220, LINEARBLEND);
        // trail
        if (ballPos > laneStart) nblend(tpl[ballPos - 1], ballClr, 80);
        if (ballPos > laneStart + 1) nblend(tpl[ballPos - 2], ballClr, 40);
        // head
        tpl[ballPos] = ballClr;

        // Move ball and handle collisions
        const uint16_t prevPos = ballPos;
        ballPos = (uint16_t)min<uint32_t>(laneEnd + 2, (uint32_t)ballPos + ballVel);
        // collision: if ball reaches or passes a standing pin, knock it down
        for (uint8_t i = 0; i < kPins; i++) {
            if (!pinUp[i]) continue;
            const uint8_t p = pinPos[i];
            if ((prevPos <= p) && (ballPos >= p)) {
                pinUp[i] = false;
                pinSpark[i] = 255;
                // splash around the pin
                if (p > laneStart) tpl[p - 1] += CRGB::White;
                if (p < laneEnd) tpl[p + 1] += CRGB::White;
            }
        }

        // End conditions: ball off lane or all pins down
        bool allDown = true;
        for (uint8_t i = 0; i < kPins; i++) if (pinUp[i]) { allDown = false; break; }
        if (ballPos >= laneEnd + 1 || allDown) {
            // allow sparkles to fade for a short hold, then reset
            if (millis() - lastReset > resetHoldMs) {
                resetFrame(true);
            }
        } else {
            lastReset = millis();
        }

        replicateMirrorSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

uint8_t FxI6::selectionWeight() const { return 9; }
