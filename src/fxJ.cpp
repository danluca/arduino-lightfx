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

void FxJ::fxRegister() {
    fxRegistry.registerEffect(&fxj1Desc);
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
    for (auto &k : kernels)
        k.pos256 = -1;
    flash = 0;
    tpl.fill_solid(bgColor);
}

void FxJ1::pop(Kernel &k) {
    k.pos256 = 0;
    k.vel256 = maxVel256 * random8(66, 100) / 100;
    k.colorIdx = random8();
    flash = 255;
}

void FxJ1::run() {
    EVERY_N_MILLISECONDS(20) {
        const uint16_t size = tpl.size();
        CRGBSet revTpl = -tpl;
        CRGBSet &frame = dirRight ? tpl : revTpl;
        frame.fill_solid(bgColor);

        for (auto &k : kernels) {
            if (k.pos256 < 0) {
                if (random8() < popChance)
                    pop(k);
                continue;
            }
            k.pos256 += k.vel256;
            k.vel256 -= kGravity256;
            if (k.pos256 < 0)
                continue;   // fell back to the floor - kernel becomes inactive

            // anti-aliased draw across the two pixels straddling the kernel position
            const auto idx = static_cast<uint16_t>(k.pos256 >> 8);
            const auto frac = static_cast<uint8_t>(k.pos256 & 0xFF);
            const CRGB col = ColorFromPalette(palette, k.colorIdx, 255, LINEARBLEND);
            if (idx < size) {
                CRGB c = col; c.nscale8_video(255 - frac);
                frame[idx] += c;
            }
            if (idx + 1 < size) {
                CRGB c = col; c.nscale8_video(frac);
                frame[idx + 1] += c;
            }
        }

        // brief white flash at the floor marks each pop
        if (flash > 0) {
            frame[0] += CRGB(flash, flash, flash);
            flash = qsub8(flash, 64);
        }

        replicateMirrorSet(frame, others, dirRight);
        FastLED.show(stripBrightness);
    }
}

uint8_t FxJ1::selectionWeight() const {
    return 8;
}
