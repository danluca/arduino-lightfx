//
// Copyright (c) by Dan Luca. All rights reserved
//
/**
 * Category J of light effects
 *
 */
#include "fxJ.h"

using namespace FxJ;

//~ Effect description strings stored in flash
static const EffectInfo fxj1Desc = {EFFECT_FACTORY(FxJ1), "FXJ1", "Popcorn", 8};
static const EffectInfo fxj2Desc = {EFFECT_FACTORY(FxJ2), "FXJ2", "Rain on a window", 8};

void FxJ::fxRegister() {
    fxRegistry.registerEffect(&fxj1Desc);
    fxRegistry.registerEffect(&fxj2Desc);
}

// FXJ1 - Popcorn
// Ported from https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/Popcorn.h
// Copyright (c) 2018 Keith Lord, MIT License
FxJ1::FxJ1() : LedEffect(fxj1Desc) {}

// gravity per tick, 8 bits fraction - original uses 0.1 pixels/tick^2
static constexpr int32_t kGravity256 = 26;

void FxJ1::setup() {
    LedEffect::setup();
    dirRight = random8() & 1;
    popChance = random8(2, 5);
    bgColor = ColorFromPalette(targetPalette, random8(), 8, LINEARBLEND);
    // launch velocity coefficient from the original effect (the "secret sauce") - peaks at ~3/4 of the frame height
    const float coeff = powf(static_cast<float>(tpl.size()), 0.5223324f) * 0.3944296f;
    maxVel256 = static_cast<int32_t>(coeff * 256.0f);
    // kernels rest within the bottom few pixels - at most 3, at most 1/8 of the frame
    maxRest256 = static_cast<int32_t>(min<uint16_t>(3, tpl.size() / 8)) << 8;
    for (auto &k : kernels) {
        k.pos256 = -1;
        k.warm = 0;
    }
    flash = 0;
    tpl.fill_solid(bgColor);
}

// kernel starts heating up - it sits at a resting spot near the floor and wiggles until it pops
void FxJ1::heat(Kernel &k) {
    k.rest256 = maxRest256 > 0 ? static_cast<int32_t>(random16(maxRest256)) : 0;
    k.colorIdx = random8();
    k.warmTotal = k.warm = random8(15, 45);     // 0.3s - 0.9s at 20ms per tick
    k.phase = random8();
}

void FxJ1::pop(Kernel &k) {
    k.pos256 = k.rest256;
    k.vel256 = maxVel256 * random8(66, 100) / 100;
    flash = 255;
    flashIdx = static_cast<uint16_t>(k.rest256 >> 8);
}

void FxJ1::run() {
    EVERY_N_MILLISECONDS(20) {
        const uint16_t size = tpl.size();
        CRGBSet revTpl = -tpl;
        CRGBSet &frame = dirRight ? tpl : revTpl;
        frame.fill_solid(bgColor);

        // anti-aliased draw across the two pixels straddling the position
        auto draw = [&](int32_t pos256, const CRGB &col) {
            if (pos256 < 0)
                pos256 = 0;
            const auto idx = static_cast<uint16_t>(pos256 >> 8);
            const auto frac = static_cast<uint8_t>(pos256 & 0xFF);
            if (idx < size) {
                CRGB c = col; c.nscale8_video(255 - frac);
                frame[idx] += c;
            }
            if (idx + 1 < size) {
                CRGB c = col; c.nscale8_video(frac);
                frame[idx + 1] += c;
            }
        };

        for (auto &k : kernels) {
            if (k.warm > 0) {
                // wiggle - oscillation grows faster, wider and brighter as the kernel heats up
                const uint8_t progress = 255 - static_cast<uint8_t>(k.warm * 255 / k.warmTotal);
                k.phase += 24 + scale8(progress, 72);
                const int32_t amp256 = 32 + scale8(progress, 112);     // up to ~0.55 pixels
                const int32_t offset = (static_cast<int32_t>(sin8(k.phase)) - 128) * amp256 / 128;
                const uint8_t bri = 96 + scale8(progress, 159);
                draw(k.rest256 + offset, ColorFromPalette(palette, k.colorIdx, bri, LINEARBLEND));
                if (--k.warm == 0)
                    pop(k);
                continue;
            }
            if (k.pos256 < 0) {
                if (random8() < popChance)
                    heat(k);
                continue;
            }
            k.pos256 += k.vel256;
            k.vel256 -= kGravity256;
            if (k.pos256 < 0)
                continue;   // fell back to the floor - kernel becomes inactive

            draw(k.pos256, ColorFromPalette(palette, k.colorIdx, 255, LINEARBLEND));
        }

        // brief white flash marks each pop
        if (flash > 0) {
            if (flashIdx < size)
                frame[flashIdx] += CRGB(flash, flash, flash);
            flash = qsub8(flash, 64);
        }

        replicateMirrorSet(frame, others, dirRight);
        FastLED.show(stripBrightness);
    }
}

uint8_t FxJ1::selectionWeight() const {
    return 8;
}

// FXJ2 - Rain on a window
// Inspired by https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/Rain.h
// Copyright (c) 2018 Keith Lord, MIT License
FxJ2::FxJ2() : LedEffect(fxj2Desc) {}

void FxJ2::setup() {
    LedEffect::setup();
    dirRight = random8() & 1;
    bpm = random8(1, 4);
    bgColor = ColorFromPalette(targetPalette, random8(), 6, LINEARBLEND);
    for (auto &d : drops)
        d.pos256 = -1;
    memset(wet, 0, sizeof(wet));
    tpl.fill_solid(bgColor);
}

// a drop lands on the glass at the given position - small drops are more likely than big ones
void FxJ2::spawn(const int32_t pos256, const uint8_t mass, const bool splash) {
    for (auto &d : drops) {
        if (d.pos256 >= 0)
            continue;
        d.pos256 = pos256;
        d.vel256 = 0;
        d.mass = mass;
        d.colorIdx = random8();
        d.bri = 255;
        d.flash = splash ? 255 : 0;
        // heavier drops cling for a shorter time; mass 1 droplets cling until they evaporate or get swept up
        d.hold = mass < 2 ? UINT16_MAX : random16(25, 150) * (maxMass + 1 - mass) / maxMass + 10;
        return;
    }
}

// a drop that catches up with another one swallows it - the combined drop is heavier and keeps sliding
void FxJ2::merge() {
    for (auto &a : drops) {
        if (a.pos256 < 0)
            continue;
        for (auto &b : drops) {
            if (&a == &b || b.pos256 < 0 || b.pos256 > a.pos256 || a.pos256 - b.pos256 >= 256)
                continue;
            // b is just below a - fold a into b
            if (a.mass > b.mass)
                b.colorIdx = a.colorIdx;
            b.mass = min<uint8_t>(a.mass + b.mass, maxMass);
            b.vel256 = max(a.vel256, b.vel256);
            b.hold = min(a.hold, b.hold);
            if (b.mass >= 2 && b.hold == UINT16_MAX)
                b.hold = random8(10, 40);   // droplets that coalesce become heavy enough to slide
            b.bri = 255;
            a.pos256 = -1;
            break;
        }
    }
}

void FxJ2::run() {
    EVERY_N_MILLISECONDS(20) {
        ++tick;
        const uint16_t size = tpl.size();
        CRGBSet revTpl = -tpl;
        CRGBSet &wframe = dirRight ? tpl : revTpl;

        // new drops hit the glass - intensity ebbs and flows between drizzle and shower
        if (random8() < beatsin8(bpm, 3, 24)) {
            const uint8_t mass = min(random8(1, maxMass + 1), random8(1, maxMass + 1));
            const auto pos256 = static_cast<int32_t>(random16(2, size)) << 8;
            spawn(pos256, mass, true);
            // impact spatter - a tiny droplet lands next to the drop
            if (mass > 2 && random8() < 128) {
                const int32_t off = static_cast<int32_t>(random8(1, 3)) << 8;
                const int32_t spatter = random8() & 1 ? pos256 + off : pos256 - off;
                if (spatter >= 0 && spatter < (static_cast<int32_t>(size) << 8))
                    spawn(spatter, 1, false);
            }
        }

        for (auto &d : drops) {
            if (d.pos256 < 0)
                continue;
            if (d.hold > 0) {
                if (d.hold != UINT16_MAX)
                    --d.hold;
                else if ((tick & 0x03) == 0) {
                    // tiny droplets slowly evaporate
                    d.bri = qsub8(d.bri, 3);
                    if (d.bri < 24)
                        d.pos256 = -1;
                }
                continue;
            }
            // sliding - drops run faster along an already wet path and stall less often on it
            const auto idx = static_cast<uint16_t>(d.pos256 >> 8);
            const bool onWet = wet[idx] > 32;
            const int32_t terminal = d.mass * (onWet ? 18 : 12);
            d.vel256 = min(d.vel256 + 1 + d.mass / 2, terminal);
            if (random8() < (onWet ? 3 : 10)) {
                // stick-slip - the drop snags on the glass for a moment
                d.vel256 /= 4;
                if (random8() < 64)
                    d.hold = random8(5, 30);
            }
            d.pos256 -= d.vel256;
            if (d.pos256 < 0) {
                wet[0] = qadd8(wet[0], 40);    // drop reaches the bottom of the window
                continue;
            }
            const auto nIdx = static_cast<uint16_t>(d.pos256 >> 8);
            if (nIdx != idx) {
                wet[nIdx] = max<uint8_t>(wet[nIdx], 40 + d.mass * 15);
                wetIdx[nIdx] = d.colorIdx;
                // the drop leaves some of its water behind on the trail, until it is too small to slide
                if (random8() < 6 && --d.mass < 2) {
                    d.mass = 1;
                    d.vel256 = 0;
                    d.hold = UINT16_MAX;
                }
            }
        }
        merge();

        // render - glass background, wet trails, then the drops themselves
        wframe.fill_solid(bgColor);
        for (uint16_t i = 0; i < size; i++) {
            if (wet[i] == 0)
                continue;
            wframe[i] += ColorFromPalette(palette, wetIdx[i], wet[i], LINEARBLEND);
            if (tick & 0x01)
                wet[i]--;
        }

        // anti-aliased draw across the two pixels straddling the position
        auto draw = [&](const int32_t pos256, const CRGB &col) {
            const auto idx = static_cast<uint16_t>(pos256 >> 8);
            const auto frac = static_cast<uint8_t>(pos256 & 0xFF);
            if (idx < size) {
                CRGB c = col; c.nscale8_video(255 - frac);
                wframe[idx] += c;
            }
            if (idx + 1 < size) {
                CRGB c = col; c.nscale8_video(frac);
                wframe[idx + 1] += c;
            }
        };

        for (auto &d : drops) {
            if (d.pos256 < 0)
                continue;
            const uint8_t bri = scale8(d.bri, 120 + d.mass * 22);
            const CRGB col = ColorFromPalette(palette, d.colorIdx, bri, LINEARBLEND);
            draw(d.pos256, col);
            // big drops elongate as they slide - a dimmer body trails above the head
            if (d.mass > 3 && d.hold == 0)
                draw(d.pos256 + 256, CRGB(col).nscale8_video(85));
            // splash on impact - a white flash spreading briefly to the neighbors
            if (d.flash > 0) {
                const auto idx = static_cast<uint16_t>(d.pos256 >> 8);
                const uint8_t side = d.flash / 4;
                wframe[idx] += CRGB(d.flash, d.flash, d.flash);
                if (idx > 0)
                    wframe[idx - 1] += CRGB(side, side, side);
                if (idx + 1 < size)
                    wframe[idx + 1] += CRGB(side, side, side);
                d.flash = qsub8(d.flash, 40);
            }
        }

        replicateMirrorSet(wframe, others, dirRight);
        FastLED.show(stripBrightness);
    }
}

uint8_t FxJ2::selectionWeight() const {
    return 8;
}
