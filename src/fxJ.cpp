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
static const EffectInfo fxj1Desc = {.factory = EFFECT_FACTORY(FxJ1), .desc = {.id = "FXJ1", .description = "Popcorn"}, .selectionWeight = 16};
static constexpr HolidayWeight fxj2HolidayWeights[] = {
    {.holiday = Halloween, .weight = 36},
    {.holiday = ValentineDay, .weight = 0},
    {.holiday = Christmas, .weight = 0}
};
static const EffectInfo fxj2Desc = {.factory = EFFECT_FACTORY(FxJ2), .desc = {.id = "FXJ2", .description = "Rain on a window"}, .selectionWeight = 20, HOLIDAY_WEIGHTS(fxj2HolidayWeights)};

static constexpr HolidayWeight fxj3HolidayWeights[] = {
    {.holiday = MemorialDay, .weight = 16},
    {.holiday = IndependenceDay, .weight = 16}
};
static const EffectInfo fxj3Desc = {.factory = EFFECT_FACTORY(FxJ3), .desc = {.id = "FXJ3", .description = "1D Wave Equation - Fluid water ripple"}, .selectionWeight = 16, HOLIDAY_WEIGHTS(fxj3HolidayWeights)};

static constexpr HolidayWeight fxj4HolidayWeights[] = {
    {.holiday = Party, .weight = 24},
    {.holiday = IndependenceDay, .weight = 30},
    {.holiday = NewYear, .weight = 28}
};
static const EffectInfo fxj4Desc = {.factory = EFFECT_FACTORY(FxJ4), .desc = {.id = "FXJ4", .description = "Meteor shower with thermal sparks"}, .selectionWeight = 18, HOLIDAY_WEIGHTS(fxj4HolidayWeights)};

void FxJ::fxRegister() {
    fxRegistry.registerEffect(&fxj1Desc);
    fxRegistry.registerEffect(&fxj2Desc);
    fxRegistry.registerEffect(&fxj3Desc);
    fxRegistry.registerEffect(&fxj4Desc);
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
    bgColor = ColorFromPalette(targetPalette, random8(), 6, LINEARBLEND);
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
    EVERY_N_MILLIS_I(j1Timer, 25) {
        const uint16_t size = tpl.size();
        CRGBSet revTpl = -tpl;
        CRGBSet &wFrame = dirRight ? tpl : revTpl;
        wFrame.fill_solid(bgColor);

        // anti-aliased draw across the two pixels straddling the position
        auto draw = [&](int32_t pos256, const CRGB &col) {
            if (pos256 < 0)
                pos256 = 0;
            const auto idx = static_cast<uint16_t>(pos256 >> 8);
            const auto frac = static_cast<uint8_t>(pos256 & 0xFF);
            if (idx < size) {
                CRGB c = col; c.nscale8_video(255 - frac);
                wFrame[idx] += c;
            }
            if (idx + 1 < size) {
                CRGB c = col; c.nscale8_video(frac);
                wFrame[idx + 1] += c;
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
                wFrame[flashIdx] += CRGB(flash, flash, flash);
            flash = qsub8(flash, 64);
        }

        replicateMirrorSet(wFrame, others, dirRight);
        FastLED.show(stripBrightness);
        j1Timer.setPeriod(random16(20, 64));
    }
}

// FXJ2 - Rain on a window
// Inspired by https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/Rain.h
// Copyright (c) 2018 Keith Lord, MIT License
FxJ2::FxJ2() : LedEffect(fxj2Desc) {}

void FxJ2::setup() {
    LedEffect::setup();
    wetTrail = fx::effectScratchBytes(0, tpl.size());
    wetTrailColors = fx::effectScratchBytes(tpl.size(), tpl.size());
    if (!wetTrail || !wetTrailColors) {
        log_error(F("FxJ2 cannot acquire %hu scratch bytes per trail"), tpl.size());
        return;
    }
    dirRight = random8() & 1;
    bpm = random8(1, 4);
    bgColor = ColorFromPalette(targetPalette, random8(), 6, LINEARBLEND);
    for (auto &d : drops)
        d.pos256 = -1;
    memset(wetTrail.data, 0, wetTrail.size);
    memset(wetTrailColors.data, 0, wetTrailColors.size);
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
    if (!wetTrail || !wetTrailColors)
        return;
    EVERY_N_MILLIS_I(j2Timer, 25) {
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
            if (idx >= size) {
                d.pos256 = -1;
                continue;
            }
            const bool onWet = wetTrail.data[idx] > 32;
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
                wetTrail.data[0] = qadd8(wetTrail.data[0], 40);    // drop reaches the bottom of the window
                continue;
            }
            const auto nIdx = static_cast<uint16_t>(d.pos256 >> 8);
            if (nIdx != idx) {
                if (nIdx >= size) {
                    d.pos256 = -1;
                    continue;
                }
                wetTrail.data[nIdx] = max<uint8_t>(wetTrail.data[nIdx], 40 + d.mass * 15);
                wetTrailColors.data[nIdx] = d.colorIdx;
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
            if (wetTrail.data[i] == 0)
                continue;
            wframe[i] += ColorFromPalette(palette, wetTrailColors.data[i], wetTrail.data[i], LINEARBLEND);
            if (tick & 0x01)
                wetTrail.data[i]--;
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
                if (idx < size)
                    wframe[idx] += CRGB(d.flash, d.flash, d.flash);
                if (idx > 0 && idx < size)
                    wframe[idx - 1] += CRGB(side, side, side);
                if (idx + 1 < size)
                    wframe[idx + 1] += CRGB(side, side, side);
                d.flash = qsub8(d.flash, 40);
            }
        }

        replicateMirrorSet(wframe, others, dirRight);
        FastLED.show(stripBrightness);

        j2Timer.setPeriod(random16(20, 64));
    }
}

// FXJ3 - 1D Wave Equation (Fluid Ripples / Plucked String)
FxJ3::FxJ3() : LedEffect(fxj3Desc) {}

void FxJ3::setup() {
    LedEffect::setup();
    heightView = effectScratchBytes(0, tpl.size());
    velView = effectScratchBytes(tpl.size(), tpl.size());
    if (heightView)
        memset(heightView.data, 0, heightView.size);
    if (velView)
        memset(velView.data, 0, velView.size);

    dropChance = random8(18, 40);
    colorBase = random8();
    bgColor = ColorFromPalette(targetPalette, random8(), 4, LINEARBLEND);
    tpl.fill_solid(bgColor);
}

void FxJ3::injectDrop() {
    if (!heightView || tpl.size() < 3)
        return;
    const auto *h = reinterpret_cast<int8_t *>(heightView.data);
    const auto pos = random16(1, tpl.size() - 1);
    const int8_t impact = (random8() & 1) ? static_cast<int8_t>(random8(80, 127)) : static_cast<int8_t>(-static_cast<int8_t>(random8(80, 127)));
    auto *mutableH = const_cast<int8_t *>(h);
    mutableH[pos] = impact;
    if (pos > 0)
        mutableH[pos - 1] = impact / 2;
    if (pos + 1 < tpl.size())
        mutableH[pos + 1] = impact / 2;
}

void FxJ3::run() {
    if (!heightView || !velView)
        return;

    EVERY_N_MILLIS_I(j3Timer, 30) {
        const uint16_t size = tpl.size();
        auto *h = reinterpret_cast<int8_t *>(heightView.data);
        auto *v = reinterpret_cast<int8_t *>(velView.data);

        if (random8() < dropChance)
            injectDrop();

        // 1D discrete wave equation update
        for (uint16_t i = 1; i + 1 < size; ++i) {
            const int16_t force = (static_cast<int16_t>(h[i - 1]) + static_cast<int16_t>(h[i + 1]) - (static_cast<int16_t>(h[i]) << 1)) / 4;
            const int16_t newV = (static_cast<int16_t>(v[i]) + force) * 251 / 256;
            v[i] = static_cast<int8_t>(constrain(newV, -127, 127));
        }

        // Boundary reflection with damping
        v[0] = static_cast<int8_t>(constrain(((static_cast<int16_t>(v[0]) + (static_cast<int16_t>(h[1]) - static_cast<int16_t>(h[0])) / 4) * 251 / 256), -127, 127));
        v[size - 1] = static_cast<int8_t>(constrain(((static_cast<int16_t>(v[size - 1]) + (static_cast<int16_t>(h[size - 2]) - static_cast<int16_t>(h[size - 1])) / 4) * 251 / 256), -127, 127));

        for (uint16_t i = 0; i < size; ++i) {
            const int16_t newH = static_cast<int16_t>(h[i]) + (static_cast<int16_t>(v[i]) >> 1);
            h[i] = static_cast<int8_t>(constrain(newH, -127, 127));
        }

        // Render wave displacement and slope reflection
        tpl.fill_solid(bgColor);
        for (uint16_t i = 0; i < size; ++i) {
            const int16_t disp = abs(static_cast<int16_t>(h[i]));
            int16_t slope = 0;
            if (i > 0 && i + 1 < size)
                slope = abs(static_cast<int16_t>(h[i + 1]) - static_cast<int16_t>(h[i - 1]));

            const uint8_t bri = qadd8(static_cast<uint8_t>(disp * 2), static_cast<uint8_t>(slope * 2));
            if (bri > 6) {
                const uint8_t cIdx = colorBase + static_cast<uint8_t>(h[i] + 128);
                tpl[i] += ColorFromPalette(palette, cIdx, bri, LINEARBLEND);
            }
        }

        colorBase++;
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

// FXJ4 - Meteor Shower with Thermal Sparks
FxJ4::FxJ4() : LedEffect(fxj4Desc) {}

void FxJ4::setup() {
    LedEffect::setup();
    heatTrail = effectScratchBytes(0, tpl.size());
    heatColors = effectScratchBytes(tpl.size(), tpl.size());
    if (heatTrail)
        memset(heatTrail.data, 0, heatTrail.size);
    if (heatColors)
        memset(heatColors.data, 0, heatColors.size);

    dirRight = random8() & 1;
    spawnChance = random8(16, 36);
    spawnCount = 0;
    bgColor = ColorFromPalette(targetPalette, random8(), 4, LINEARBLEND);
    for (auto &m : meteors)
        m.pos256 = -1;
    for (auto &s : sparks)
        s.pos256 = -1;

    tpl.fill_solid(bgColor);
}

void FxJ4::spawnMeteor() {
    spawnCount++;
    // Periodically switch dominant shower direction (every 10-18 meteors)
    if (spawnCount >= 12 && (random8() < 64)) {
        dirRight = !dirRight;
        spawnCount = 0;
    }

    for (auto &m : meteors) {
        if (m.pos256 < 0) {
            // 85% follow the dominant direction, 15% rogue crossing meteors
            const bool meteorDirRight = (random8() < 218) ? dirRight : !dirRight;
            m.pos256 = meteorDirRight ? 0 : (static_cast<int32_t>(tpl.size() - 1) << 8);
            const int32_t speedMag = static_cast<int32_t>(random8(120, 240)) * (tpl.size() > 100 ? 3 : 2);
            m.vel256 = meteorDirRight ? speedMag : -speedMag;
            m.colorIdx = random8();
            m.trailHeat = random8(190, 255);
            m.decay = random8(220, 245);
            break;
        }
    }
}

void FxJ4::spawnSpark(const int32_t pos256, const int32_t vel256, const uint8_t colorIdx) {
    for (auto &s : sparks) {
        if (s.pos256 < 0) {
            s.pos256 = pos256;
            const int32_t kickDir = (vel256 > 0) ? -1 : 1;
            const int32_t kickback = kickDir * static_cast<int32_t>(random8(20, 80));
            s.vel256 = vel256 / 4 + kickback;
            s.colorIdx = colorIdx + random8(24);
            s.bri = random8(160, 255);
            s.decay = random8(14, 32);
            break;
        }
    }
}

void FxJ4::run() {
    if (!heatTrail || !heatColors)
        return;

    EVERY_N_MILLIS_I(j4Timer, 30) {
        const uint16_t size = tpl.size();

        // Cool the thermal heat trail
        for (uint16_t i = 0; i < size; ++i)
            heatTrail.data[i] = scale8(heatTrail.data[i], 242);

        if (random8() < spawnChance)
            spawnMeteor();

        // Update meteors
        for (auto &m : meteors) {
            if (m.pos256 < 0)
                continue;

            m.pos256 += m.vel256;
            const auto idx = static_cast<int32_t>(m.pos256 >> 8);
            if (idx < 0 || idx >= size) {
                m.pos256 = -1;
                continue;
            }

            // Leave thermal glow in trail
            heatTrail.data[idx] = max<uint8_t>(heatTrail.data[idx], m.trailHeat);
            heatColors.data[idx] = m.colorIdx;

            // Shed micro-sparks behind head
            if (random8() < 90)
                spawnSpark(m.pos256, m.vel256, m.colorIdx);
        }

        // Update sparks
        for (auto &s : sparks) {
            if (s.pos256 < 0)
                continue;

            s.pos256 += s.vel256;
            s.bri = qsub8(s.bri, s.decay);
            const auto idx = static_cast<int32_t>(s.pos256 >> 8);
            if (idx < 0 || idx >= size || s.bri < 8) {
                s.pos256 = -1;
            }
        }

        // Render base background and heat trail
        tpl.fill_solid(bgColor);
        for (uint16_t i = 0; i < size; ++i) {
            if (heatTrail.data[i] > 2)
                tpl[i] += ColorFromPalette(palette, heatColors.data[i], heatTrail.data[i], LINEARBLEND);
        }

        // Anti-aliased draw lambda
        auto draw = [&](const int32_t pos256, const CRGB &col) {
            const auto idx = static_cast<uint16_t>(pos256 >> 8);
            const auto frac = static_cast<uint8_t>(pos256 & 0xFF);
            if (idx < size) {
                CRGB c = col; c.nscale8_video(255 - frac);
                tpl[idx] += c;
            }
            if (idx + 1 < size) {
                CRGB c = col; c.nscale8_video(frac);
                tpl[idx + 1] += c;
            }
        };

        // Render sparks
        for (const auto &s : sparks) {
            if (s.pos256 < 0)
                continue;
            const CRGB col = ColorFromPalette(palette, s.colorIdx, s.bri, LINEARBLEND);
            draw(s.pos256, col);
        }

        // Render meteor heads
        for (const auto &m : meteors) {
            if (m.pos256 < 0)
                continue;
            const CRGB headCol = CRGB::White;
            draw(m.pos256, headCol);
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

