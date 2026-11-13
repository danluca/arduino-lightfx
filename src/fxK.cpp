//
// Copyright (c) by Dan Luca. All rights reserved
//
/**
 * Category K of light effects
 *
 */
#include "fxK.h"

using namespace FxK;

//~ Effect description strings stored in flash
static constexpr HolidayWeight fxk1HolidayWeights[] = {
    {.holiday = Christmas, .weight = 30},
    {.holiday = NewYear, .weight = 25},
    {.holiday = ValentineDay, .weight = 15}
};
static const EffectInfo fxk1Desc = {.factory = EFFECT_FACTORY(FxK1), .desc = {.id = "FXK1", .description = "Aurora Borealis"}, .selectionWeight = 20, HOLIDAY_WEIGHTS(fxk1HolidayWeights)};

static constexpr HolidayWeight fxk2HolidayWeights[] = {
    {.holiday = Party, .weight = 20},
    {.holiday = Halloween, .weight = 20}
};
static const EffectInfo fxk2Desc = {.factory = EFFECT_FACTORY(FxK2), .desc = {.id = "FXK2", .description = "1D Cellular Automata"}, .selectionWeight = 15, HOLIDAY_WEIGHTS(fxk2HolidayWeights)};

static constexpr HolidayWeight fxk3HolidayWeights[] = {
    {.holiday = ValentineDay, .weight = 45},
    {.holiday = Halloween, .weight = 25}
};
static const EffectInfo fxk3Desc = {.factory = EFFECT_FACTORY(FxK3), .desc = {.id = "FXK3", .description = "Organic Heartbeat"}, .selectionWeight = 16, HOLIDAY_WEIGHTS(fxk3HolidayWeights)};

static constexpr HolidayWeight fxk4HolidayWeights[] = {
    {.holiday = Party, .weight = 24},
    {.holiday = NewYear, .weight = 24}
};
static const EffectInfo fxk4Desc = {.factory = EFFECT_FACTORY(FxK4), .desc = {.id = "FXK4", .description = "N-Body Gravitational Attractors"}, .selectionWeight = 18, HOLIDAY_WEIGHTS(fxk4HolidayWeights)};

const uint8_t FxK2::rules[maxRules] = {30, 90, 110, 150};

void FxK::fxRegister() {
    fxRegistry.registerEffect(&fxk1Desc);
    fxRegistry.registerEffect(&fxk2Desc);
    fxRegistry.registerEffect(&fxk3Desc);
    fxRegistry.registerEffect(&fxk4Desc);
}

// FXK1 - Aurora Borealis
FxK1::FxK1() : LedEffect(fxk1Desc) {}

void FxK1::setup() {
    LedEffect::setup();
    noiseScale1 = random16(1200, 2400);
    noiseScale2 = random16(2800, 4800);
    noiseScale3 = random16(5000, 8000);
    xOffset1 = (static_cast<uint32_t>(random16()) << 16) | random16();
    xOffset2 = (static_cast<uint32_t>(random16()) << 16) | random16();
    xOffset3 = (static_cast<uint32_t>(random16()) << 16) | random16();
    breathPhase = random8();
    colorBase = random8();
}

void FxK1::run() {
    EVERY_N_MILLIS_I(k1Timer, 25) {
        const uint16_t size = tpl.size();
        breathPhase += 2;
        const uint8_t breath = beatsin8(14, 160, 255);
        xOffset1 += 80;
        xOffset2 -= 130;
        xOffset3 += 210;

        for (uint16_t i = 0; i < size; ++i) {
            const uint16_t n1 = inoise16(i * noiseScale1 + xOffset1, xOffset1 / 4);
            const uint16_t n2 = inoise16(i * noiseScale2 + xOffset2, xOffset2 / 2);
            const uint16_t n3 = inoise16(i * noiseScale3 + xOffset3, xOffset3);

            const uint8_t nVal1 = n1 >> 8;
            const uint8_t nVal2 = n2 >> 8;
            const uint8_t nVal3 = n3 >> 8;

            uint8_t val = qadd8(scale8(nVal1, 140), scale8(nVal2, 80));
            val = qadd8(val, scale8(nVal3, 35));
            val = scale8(val, breath);

            const uint8_t colIdx = colorBase + (nVal1 / 2) + (nVal2 / 4);
            tpl[i] = ColorFromPalette(palette, colIdx, val, LINEARBLEND);
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

// FXK2 - 1D Cellular Automata
FxK2::FxK2() : LedEffect(fxk2Desc) {}

void FxK2::setup() {
    LedEffect::setup();
    cellState = effectScratchBytes(0, tpl.size());
    cellAge = effectScratchBytes(tpl.size(), tpl.size());
    currentRuleIdx = random8(maxRules);
    genCount = 0;
    colorBase = random8();
    seedAutomaton();
    tpl.fill_solid(BKG);
}

void FxK2::seedAutomaton() {
    if (!cellState || !cellAge)
        return;
    memset(cellState.data, 0, cellState.size);
    memset(cellAge.data, 0, cellAge.size);

    const uint16_t size = tpl.size();
    cellState.data[size / 2] = 1;
    cellAge.data[size / 2] = 255;
    if (size > 60) {
        cellState.data[size / 4] = 1;
        cellAge.data[size / 4] = 255;
        cellState.data[3 * size / 4] = 1;
        cellAge.data[3 * size / 4] = 255;
    }
}

void FxK2::stepRule() {
    const uint16_t size = tpl.size();
    uint8_t left = cellState.data[size - 1] & 1;
    const uint8_t rule = rules[currentRuleIdx];

    for (uint16_t i = 0; i < size; ++i) {
        const uint8_t mid = cellState.data[i] & 1;
        const uint8_t right = cellState.data[(i + 1) % size] & 1;
        const uint8_t pattern = (left << 2) | (mid << 1) | right;
        const uint8_t next = (rule & (1 << pattern)) ? 1 : 0;
        left = mid;
        cellState.data[i] = next;
        if (next)
            cellAge.data[i] = qadd8(cellAge.data[i], 16);
        else
            cellAge.data[i] = scale8(cellAge.data[i], 200);
    }
}

void FxK2::run() {
    if (!cellState || !cellAge)
        return;

    EVERY_N_MILLIS_I(k2Timer, 50) {
        const uint16_t size = tpl.size();
        stepRule();
        genCount++;

        uint16_t activeCount = 0;
        for (uint16_t i = 0; i < size; ++i) {
            if (cellState.data[i])
                activeCount++;
        }

        if (activeCount == 0 || activeCount == size || genCount > 200) {
            currentRuleIdx = (currentRuleIdx + 1) % maxRules;
            seedAutomaton();
            genCount = 0;
            colorBase += 32;
        }

        for (uint16_t i = 0; i < size; ++i) {
            const uint8_t age = cellAge.data[i];
            if (cellState.data[i]) {
                tpl[i] = ColorFromPalette(palette, colorBase + age, 255, LINEARBLEND);
            } else if (age > 4) {
                tpl[i] = ColorFromPalette(palette, colorBase + age + 64, age, LINEARBLEND);
            } else {
                tpl[i] = BKG;
            }
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

// FXK3 - Organic Heartbeat
FxK3::FxK3() : LedEffect(fxk3Desc) {}

void FxK3::setup() {
    LedEffect::setup();
    phase = Diastole;
    phaseTimer = 0;
    wavePos1_256 = 0;
    wavePos2_256 = 0;
    waveBri1 = 0;
    waveBri2 = 0;
    centerGlow = 0;
    colorBase = random8();
    s2Active = false;
    tpl.fill_solid(BKG);
}

void FxK3::run() {
    EVERY_N_MILLIS_I(k3Timer, 30) {
        const uint16_t size = tpl.size();
        const uint16_t center = size / 2;

        phaseTimer++;
        switch (phase) {
            case Diastole:
                if (phaseTimer >= 38) {
                    phase = SystoleS1;
                    phaseTimer = 0;
                    wavePos1_256 = 0;
                    waveBri1 = 255;
                    centerGlow = 255;
                }
                break;
            case SystoleS1:
                phase = Pause;
                phaseTimer = 0;
                break;
            case Pause:
                if (phaseTimer >= 6) {
                    phase = SystoleS2;
                    phaseTimer = 0;
                    wavePos2_256 = 0;
                    waveBri2 = 210;
                    centerGlow = 220;
                    s2Active = true;
                    colorBase++;
                }
                break;
            case SystoleS2:
                phase = Diastole;
                phaseTimer = 0;
                break;
        }

        // Propagate arterial shockwaves
        wavePos1_256 += 180;
        waveBri1 = scale8(waveBri1, 244);

        if (s2Active) {
            wavePos2_256 += 220;
            waveBri2 = scale8(waveBri2, 240);
            if (waveBri2 < 8)
                s2Active = false;
        }

        centerGlow = scale8(centerGlow, 238);

        // Capillary relaxation
        tpl.nscale8(232);

        // Pacemaker center node glow
        if (center < size)
            tpl[center] += ColorFromPalette(palette, colorBase, centerGlow, LINEARBLEND);
        if (center > 0)
            tpl[center - 1] += ColorFromPalette(palette, colorBase, centerGlow / 2, LINEARBLEND);
        if (center + 1 < size)
            tpl[center + 1] += ColorFromPalette(palette, colorBase, centerGlow / 2, LINEARBLEND);

        // Arterial wave 1
        const uint16_t w1Offset = wavePos1_256 >> 8;
        if (w1Offset <= center + 2) {
            if (center >= w1Offset)
                tpl[center - w1Offset] += ColorFromPalette(palette, colorBase + 16, waveBri1, LINEARBLEND);
            if (center + w1Offset < size)
                tpl[center + w1Offset] += ColorFromPalette(palette, colorBase + 16, waveBri1, LINEARBLEND);
        }

        // Arterial wave 2
        if (s2Active) {
            const uint16_t w2Offset = wavePos2_256 >> 8;
            if (w2Offset <= center + 2) {
                if (center >= w2Offset)
                    tpl[center - w2Offset] += ColorFromPalette(palette, colorBase + 48, waveBri2, LINEARBLEND);
                if (center + w2Offset < size)
                    tpl[center + w2Offset] += ColorFromPalette(palette, colorBase + 48, waveBri2, LINEARBLEND);
            }
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

// FXK4 - N-Body Gravitational Attractors
FxK4::FxK4() : LedEffect(fxk4Desc) {}

void FxK4::setup() {
    LedEffect::setup();
    bgColor = ColorFromPalette(targetPalette, random8(), 4, LINEARBLEND);
    colorBase = random8();

    const uint16_t size = tpl.size();
    attractors[0] = {.pos256 = static_cast<int32_t>((size / 4) << 8), .vel256 = 48, .mass = 120, .colorIdx = 0};
    attractors[1] = {.pos256 = static_cast<int32_t>((3 * size / 4) << 8), .vel256 = -64, .mass = 150, .colorIdx = 85};
    attractors[2] = {.pos256 = static_cast<int32_t>((size / 2) << 8), .vel256 = 32, .mass = 90, .colorIdx = 170};

    for (uint8_t i = 0; i < numParticles; ++i) {
        particles[i].pos256 = static_cast<int32_t>(random16(size) << 8);
        particles[i].vel256 = static_cast<int32_t>(random8(10, 40)) * ((i & 1) ? 1 : -1);
        particles[i].colorIdx = random8();
        particles[i].bri = random8(180, 255);
    }

    tpl.fill_solid(bgColor);
}

void FxK4::run() {
    EVERY_N_MILLIS_I(k4Timer, 30) {
        const uint16_t size = tpl.size();
        const int32_t maxPos256 = static_cast<int32_t>(size - 1) << 8;

        // Update attractors
        for (auto &a : attractors) {
            a.pos256 += a.vel256;
            if (a.pos256 <= 0) {
                a.pos256 = 0;
                a.vel256 = -a.vel256;
            }
            if (a.pos256 >= maxPos256) {
                a.pos256 = maxPos256;
                a.vel256 = -a.vel256;
            }
        }

        // Update particles under gravitational attraction
        for (auto &p : particles) {
            int32_t accel256 = 0;
            for (const auto &a : attractors) {
                const int32_t dx = (a.pos256 - p.pos256) >> 8;
                const int32_t distSq = dx * dx + 16;
                const int32_t force = (a.mass * dx * 256) / distSq;
                accel256 += force;
            }
            p.vel256 = constrain(p.vel256 + accel256 / 32, -384, 384);
            p.pos256 += p.vel256;
            if (p.pos256 < 0) {
                p.pos256 = 0;
                p.vel256 = -p.vel256 / 2;
                p.colorIdx += random8(9);
            }
            if (p.pos256 > maxPos256) {
                p.pos256 = maxPos256;
                p.vel256 = -p.vel256 / 2;
                p.colorIdx += random8(5);
            }
        }

        // Fade previous particle trails
        tpl.nscale8(200);

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

        // Render particles
        for (const auto &p : particles) {
            const CRGB col = ColorFromPalette(palette, p.colorIdx, p.bri, LINEARBLEND);
            draw(p.pos256, col);
        }

        // Render pulsating attractors
        const uint8_t pulse = beatsin8(30, 180, 255);
        for (const auto &a : attractors) {
            const CRGB col = ColorFromPalette(palette, a.colorIdx, pulse, LINEARBLEND);
            draw(a.pos256, col);
            if (a.pos256 >= 256)
                draw(a.pos256 - 256, CRGB(col).nscale8_video(96));
            if (a.pos256 + 256 <= maxPos256)
                draw(a.pos256 + 256, CRGB(col).nscale8_video(96));
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}