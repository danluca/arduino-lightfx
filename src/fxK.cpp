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

static constexpr HolidayWeight fxk5HolidayWeights[] = {
    {.holiday = Thanksgiving, .weight = 35},
    {.holiday = Halloween, .weight = 30},
    {.holiday = Christmas, .weight = 20}
};
static const EffectInfo fxk5Desc = {.factory = EFFECT_FACTORY(FxK5), .desc = {.id = "FXK5", .description = "Thermal Diffusion / Heat Shimmer"}, .selectionWeight = 16, HOLIDAY_WEIGHTS(fxk5HolidayWeights)};

static constexpr HolidayWeight fxk6HolidayWeights[] = {
    {.holiday = Christmas, .weight = 45},
    {.holiday = Thanksgiving, .weight = 40},
    {.holiday = ValentineDay, .weight = 30},
    {.holiday = Halloween, .weight = 20}
};
static const EffectInfo fxk6Desc = {.factory = EFFECT_FACTORY(FxK6), .desc = {.id = "FXK6", .description = "Distributed Candle Clusters"}, .selectionWeight = 18, HOLIDAY_WEIGHTS(fxk6HolidayWeights)};

static constexpr HolidayWeight fxk7HolidayWeights[] = {
    {.holiday = MemorialDay, .weight = 20},
    {.holiday = IndependenceDay, .weight = 20},
    {.holiday = Party, .weight = 18}
};
static const EffectInfo fxk7Desc = {.factory = EFFECT_FACTORY(FxK7), .desc = {.id = "FXK7", .description = "Prismatic Caustics"}, .selectionWeight = 18, HOLIDAY_WEIGHTS(fxk7HolidayWeights)};

static constexpr HolidayWeight fxk8HolidayWeights[] = {
    {.holiday = Party, .weight = 25},
    {.holiday = Halloween, .weight = 28},
    {.holiday = NewYear, .weight = 22}
};
static const EffectInfo fxk8Desc = {.factory = EFFECT_FACTORY(FxK8), .desc = {.id = "FXK8", .description = "Neural Synapse Pulse"}, .selectionWeight = 17, HOLIDAY_WEIGHTS(fxk8HolidayWeights)};

const uint8_t FxK2::rules[maxRules] = {30, 90, 110, 150, 73, 18};

void FxK::fxRegister() {
    fxRegistry.registerEffect(&fxk1Desc);
    fxRegistry.registerEffect(&fxk2Desc);
    fxRegistry.registerEffect(&fxk3Desc);
    fxRegistry.registerEffect(&fxk4Desc);
    fxRegistry.registerEffect(&fxk5Desc);
    fxRegistry.registerEffect(&fxk6Desc);
    fxRegistry.registerEffect(&fxk7Desc);
    fxRegistry.registerEffect(&fxk8Desc);
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
    stepTimer = 0;
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
    if (size == 0)
        return;
    uint8_t prev = cellState.data[size - 1] & 1;
    const uint8_t rule = rules[currentRuleIdx];

    for (uint16_t i = 0; i < size; ++i) {
        const uint8_t curr = cellState.data[i] & 1;
        const uint8_t next = cellState.data[(i + 1) % size] & 1;
        const uint8_t pattern = (prev << 2) | (curr << 1) | next;
        const uint8_t val = (rule & (1 << pattern)) ? 1 : 0;
        prev = curr;
        cellState.data[i] = val;
    }
}

void FxK2::run() {
    if (!cellState || !cellAge)
        return;

    EVERY_N_MILLIS_I(k2Timer, 25) {
        const uint16_t size = tpl.size();
        stepTimer++;
        if (stepTimer >= 6) { // Step cellular automata every ~150ms for deliberate, organic cell growth
            stepTimer = 0;
            stepRule();
            genCount++;

            uint16_t activeCount = 0;
            for (uint16_t i = 0; i < size; ++i) {
                if (cellState.data[i])
                    activeCount++;
            }

            if (activeCount == 0 || activeCount == size || genCount > 140) {
                currentRuleIdx = (currentRuleIdx + 1) % maxRules;
                seedAutomaton();
                genCount = 0;
                colorBase += 36;
            }
        }

        // Smooth growth and decay of cell brightness/age
        for (uint16_t i = 0; i < size; ++i) {
            if (cellState.data[i]) {
                cellAge.data[i] = qadd8(cellAge.data[i], 38);
            } else {
                cellAge.data[i] = scale8(cellAge.data[i], 215);
            }
        }

        colorBase++;

        for (uint16_t i = 0; i < size; ++i) {
            const uint8_t age = cellAge.data[i];
            if (age > 4) {
                const uint8_t cIdx = colorBase + (i * 256 / size) + (cellState.data[i] ? 0 : 28);
                tpl[i] = ColorFromPalette(palette, cIdx, age, LINEARBLEND);
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
    const uint16_t size = tpl.size();
    pulseOrigin256 = static_cast<int32_t>(size / 2) << 8;
    targetOrigin256 = pulseOrigin256;
    tpl.fill_solid(BKG);
}

void FxK3::run() {
    EVERY_N_MILLIS_I(k3Timer, 30) {
        const uint16_t size = tpl.size();

        // Smoothly glide pulse origin towards target position within central 2/3
        pulseOrigin256 += (targetOrigin256 - pulseOrigin256) / 8;
        const auto origin = static_cast<uint16_t>(constrain(pulseOrigin256 >> 8, 0, static_cast<int32_t>(size - 1)));

        phaseTimer++;
        switch (phase) {
            case Diastole:
                if (phaseTimer >= 36) {
                    phase = SystoleS1;
                    phaseTimer = 0;
                    wavePos1_256 = 0;
                    waveBri1 = 255;
                    centerGlow = 255;
                    colorBase += 24; // Dynamic palette shift on every beat

                    // Pick new pulse location within central 2/3: [size/6, 5*size/6]
                    if (size >= 6) {
                        const uint16_t minO = size / 6;
                        const uint16_t maxO = (5 * size) / 6;
                        targetOrigin256 = static_cast<int32_t>(random16(minO, maxO + 1)) << 8;
                    } else {
                        targetOrigin256 = static_cast<int32_t>(size / 2) << 8;
                    }
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
                    waveBri2 = 220;
                    centerGlow = 230;
                    s2Active = true;
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
        tpl.nscale8(230);

        // Pacemaker center node glow with rich hot core
        if (origin < size)
            tpl[origin] += ColorFromPalette(palette, colorBase, centerGlow, LINEARBLEND);
        if (origin > 0)
            tpl[origin - 1] += ColorFromPalette(palette, colorBase + 8, centerGlow / 2, LINEARBLEND);
        if (origin + 1 < size)
            tpl[origin + 1] += ColorFromPalette(palette, colorBase + 8, centerGlow / 2, LINEARBLEND);

        // Arterial wave 1 (S1 primary ventricular surge)
        const uint16_t w1Offset = wavePos1_256 >> 8;
        if (w1Offset <= size) {
            const uint8_t w1Color = colorBase + 32 + (w1Offset * 2);
            if (origin >= w1Offset)
                tpl[origin - w1Offset] += ColorFromPalette(palette, w1Color, waveBri1, LINEARBLEND);
            if (origin + w1Offset < size)
                tpl[origin + w1Offset] += ColorFromPalette(palette, w1Color, waveBri1, LINEARBLEND);
        }

        // Arterial wave 2 (S2 secondary valve closure surge with contrasting harmonic color)
        if (s2Active) {
            const uint16_t w2Offset = wavePos2_256 >> 8;
            if (w2Offset <= size) {
                const uint8_t w2Color = colorBase + 96 + (w2Offset * 2);
                if (origin >= w2Offset)
                    tpl[origin - w2Offset] += ColorFromPalette(palette, w2Color, waveBri2, LINEARBLEND);
                if (origin + w2Offset < size)
                    tpl[origin + w2Offset] += ColorFromPalette(palette, w2Color, waveBri2, LINEARBLEND);
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
    attractors[0] = {.pos256 = static_cast<int32_t>((size / 4) << 8), .vel256 = 48, .mass = 120, .colorOffset = 0};
    attractors[1] = {.pos256 = static_cast<int32_t>((3 * size / 4) << 8), .vel256 = -64, .mass = 150, .colorOffset = 85};
    attractors[2] = {.pos256 = static_cast<int32_t>((size / 2) << 8), .vel256 = 32, .mass = 90, .colorOffset = 170};

    for (uint8_t i = 0; i < numParticles; ++i) {
        particles[i].pos256 = static_cast<int32_t>(random16(size) << 8);
        particles[i].vel256 = static_cast<int32_t>(random8(10, 40)) * ((i & 1) ? 1 : -1);
        particles[i].colorIdx = random8();
        particles[i].bri = random8(200, 255);
    }

    tpl.fill_solid(bgColor);
}

void FxK4::run() {
    EVERY_N_MILLIS_I(k4Timer, 30) {
        const uint16_t size = tpl.size();
        const int32_t maxPos256 = static_cast<int32_t>(size - 1) << 8;
        colorBase++;

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
                p.colorIdx += random8(16, 48);
            }
            if (p.pos256 > maxPos256) {
                p.pos256 = maxPos256;
                p.vel256 = -p.vel256 / 2;
                p.colorIdx += random8(16, 48);
            }
        }

        // Fade previous particle trails smoothly
        tpl.nscale8(215);

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

        // Render particles with kinetic energy-driven vivid coloration
        for (const auto &p : particles) {
            const uint8_t speedHue = static_cast<uint8_t>(min<int32_t>(abs(p.vel256), 384) * 64 / 384);
            const uint8_t cIdx = colorBase + p.colorIdx + speedHue;
            const CRGB col = ColorFromPalette(palette, cIdx, p.bri, LINEARBLEND);
            draw(p.pos256, col);
        }

        // Render pulsating attractors with intense cores and chromatic halos
        const uint8_t pulse = beatsin8(26, 190, 255);
        for (const auto &a : attractors) {
            const uint8_t aColIdx = colorBase + a.colorOffset;
            const CRGB auraCol = ColorFromPalette(palette, aColIdx, pulse, LINEARBLEND);
            const CRGB coreCol = ColorFromPalette(palette, aColIdx + 16, 255, LINEARBLEND);

            // Halo glow
            if (a.pos256 >= 256)
                draw(a.pos256 - 256, CRGB(auraCol).nscale8_video(110));
            if (a.pos256 + 256 <= maxPos256)
                draw(a.pos256 + 256, CRGB(auraCol).nscale8_video(110));

            // Brilliant core
            draw(a.pos256, coreCol);
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

// FXK5 - Thermal Diffusion / Heat Shimmer
FxK5::FxK5() : LedEffect(fxk5Desc) {}

void FxK5::setup() {
    LedEffect::setup();
    heat = effectScratchBytes(0, tpl.size());
    heatNext = effectScratchBytes(tpl.size(), tpl.size());
    if (heat)
        memset(heat.data, 0, heat.size);
    if (heatNext)
        memset(heatNext.data, 0, heatNext.size);
    noisePos = random16();
    coolingRate = random8(6, 14);
    diffusionRate = random8(60, 95);
    colorBase = random8();
    tpl.fill_solid(BKG);
}

void FxK5::injectHeat() {
    const uint16_t size = tpl.size();
    if (size == 0 || !heat)
        return;

    // Periodically spawn heat injection embers / wandering thermal hotspots
    if (random8() < 70) {
        const uint16_t pos = random16(size);
        const uint8_t injection = random8(160, 255);
        heat.data[pos] = qadd8(heat.data[pos], injection);
        if (pos > 0)
            heat.data[pos - 1] = qadd8(heat.data[pos - 1], injection / 2);
        if (pos + 1 < size)
            heat.data[pos + 1] = qadd8(heat.data[pos + 1], injection / 2);
    }
}

void FxK5::run() {
    if (!heat || !heatNext)
        return;

    EVERY_N_MILLIS_I(k5Timer, 25) {
        const uint16_t size = tpl.size();
        injectHeat();
        noisePos += 24;
        colorBase++;

        // 1D discrete thermal diffusion equation with cooling dissipation
        for (uint16_t i = 0; i < size; ++i) {
            const uint8_t left = (i == 0) ? heat.data[i] : heat.data[i - 1];
            const uint8_t right = (i == size - 1) ? heat.data[i] : heat.data[i + 1];
            const int16_t diffuseFlux = static_cast<int16_t>(left) + static_cast<int16_t>(right) - (2 * static_cast<int16_t>(heat.data[i]));
            const int16_t heatVal = static_cast<int16_t>(heat.data[i]) + ((diffuseFlux * diffusionRate) / 256);
            const uint8_t clampedHeat = static_cast<uint8_t>(constrain(heatVal, 0, 255));
            heatNext.data[i] = scale8(clampedHeat, 255 - coolingRate);
        }

        // Copy forward state
        memcpy(heat.data, heatNext.data, size);

        // Render with turbulent convective draft shimmer
        for (uint16_t i = 0; i < size; ++i) {
            const uint8_t hVal = heat.data[i];
            if (hVal > 2) {
                const uint8_t draft = inoise8(i * 45 + noisePos, noisePos / 2);
                const uint8_t shimmer = qadd8(scale8(hVal, 220), scale8(draft, 35));
                const uint8_t cIdx = colorBase + (shimmer * 160 / 255);
                CRGB col = ColorFromPalette(palette, cIdx, shimmer, LINEARBLEND);
                if (shimmer > 220) {
                    const uint8_t incandescence = (shimmer - 220) * 3;
                    col += CRGB(incandescence, incandescence, incandescence);
                }
                tpl[i] = col;
            } else {
                tpl[i] = BKG;
            }
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

// FXK6 - Distributed Candle / Multi-Flame Cluster
FxK6::FxK6() : LedEffect(fxk6Desc) {}

void FxK6::initCandle(Candle &c, const uint16_t base, const uint8_t maxH) {
    c.basePos = base;
    c.maxHeight = maxH;
    // Varied starting initial heights: some candles start tall, some medium, some short
    c.height256 = static_cast<uint16_t>(random8(1, maxH + 1)) << 8;
    // Slow, realistic wax burn-down rate
    c.burnRate = random8(1, 3);
    memset(c.flameHeat, 0, sizeof(c.flameHeat));
    c.flameHeat[0] = random8(160, 240);
    c.draftPhase = random16();
    c.flareCountdown = 0;
}

void FxK6::setup() {
    LedEffect::setup();
    const uint16_t size = tpl.size();
    // Fewer, well-spaced candles per strip for calm ambiance
    activeCandles = constrain(static_cast<uint8_t>(size / 22), static_cast<uint8_t>(2), maxCandles);
    colorBase = random8();

    switch (paletteFactory.getHoliday()) {
        case Christmas:
            palette = CRGBPalette16(CRGB::Black, CRGB(180, 20, 0), CRGB(255, 180, 30), CRGB(255, 255, 220));
            break;
        case Thanksgiving:
        case Halloween:
            palette = CRGBPalette16(CRGB::Black, CRGB(180, 30, 0), CRGB(255, 120, 0), CRGB(255, 220, 100));
            break;
        default:
            palette = CRGBPalette16(CRGB::Black, CRGB(200, 30, 0), CRGB(255, 140, 0), CRGB(255, 240, 160));
            break;
    }

    const uint16_t spacing = size / activeCandles;
    for (uint8_t i = 0; i < activeCandles; ++i) {
        const uint16_t base = i * spacing + (spacing > 6 ? random8(1, spacing / 5) : 0);
        const uint8_t maxH = constrain(static_cast<uint8_t>(spacing / 4), static_cast<uint8_t>(3), static_cast<uint8_t>(8));
        initCandle(candles[i], base, maxH);
    }
    tpl.fill_solid(BKG);
}

void FxK6::run() {
    // Slower burn-down progression clock
    EVERY_N_MILLIS(250) {
        for (uint8_t i = 0; i < activeCandles; ++i) {
            Candle &c = candles[i];
            if (c.flareCountdown == 0) {
                if (c.height256 > c.burnRate) {
                    c.height256 -= c.burnRate;
                } else {
                    c.height256 = 0;
                    c.flareCountdown = 45; // ~1.3s reignition ember flare
                }
            }
        }
    }

    EVERY_N_MILLIS_I(k6Timer, 30) {
        const uint16_t size = tpl.size();
        tpl.fill_solid(BKG);

        for (uint8_t i = 0; i < activeCandles; ++i) {
            Candle &c = candles[i];

            if (c.flareCountdown > 0) {
                // Candle has burned down and is reigniting with an ember flare
                c.flareCountdown--;
                if (c.basePos < size) {
                    const uint8_t flareBri = beatsin8(50, 110, 240);
                    tpl[c.basePos] = ColorFromPalette(palette, 160, flareBri, LINEARBLEND) + CRGB(30, 10, 0);
                }
                if (c.flareCountdown == 0) {
                    // Relight candle with a fresh height and new individual burn rate
                    c.height256 = static_cast<uint16_t>(c.maxHeight) << 8;
                    c.burnRate = random8(1, 3);
                    c.flameHeat[0] = 220;
                }
                continue;
            }

            // Convective multi-cell flame physics (FXH1 Fire2012 style)
            c.draftPhase += 16;

            // Step 1: Smooth cooling across flame cells
            for (uint8_t k = 0; k < flameCells; ++k) {
                c.flameHeat[k] = qsub8(c.flameHeat[k], random8(14, 34));
            }

            // Step 2: Convective upward heat drift and spatial diffusion
            c.flameHeat[3] = (c.flameHeat[2] + c.flameHeat[1]) / 2;
            c.flameHeat[2] = (c.flameHeat[1] + c.flameHeat[0]) / 2;
            c.flameHeat[1] = (c.flameHeat[0] * 3 + c.flameHeat[1]) / 4;

            // Step 3: Spark ignition at wick base with subtle Perlin air draft modulation
            const uint8_t draft = inoise8(i * 500 + c.draftPhase);
            if (random8() < 175) {
                c.flameHeat[0] = qadd8(c.flameHeat[0], random8(140, 235));
            }
            c.flameHeat[0] = scale8(c.flameHeat[0], 215 + (draft / 7));

            // Step 4: Render wax body (steady, calming ambient beeswax glow)
            const uint8_t waxH = c.height256 >> 8;
            for (uint8_t h = 0; h < waxH; ++h) {
                const uint16_t p = c.basePos + h;
                if (p < size) {
                    tpl[p] = ColorFromPalette(palette, 35 + (h * 4), 28 + (h * 6), LINEARBLEND);
                }
            }

            // Step 5: Render convective flame plume above wax
            for (uint8_t cell = 0; cell < flameCells; ++cell) {
                const uint16_t p = c.basePos + waxH + cell;
                if (p < size) {
                    const uint8_t hVal = c.flameHeat[cell];
                    if (hVal > 15) {
                        CRGB flameCol = ColorFromPalette(palette, scale8(hVal, 240), hVal, LINEARBLEND);
                        if (hVal > 215) {
                            const uint8_t whiteCore = (hVal - 215) * 3;
                            flameCol += CRGB(whiteCore, whiteCore, whiteCore / 2);
                        }
                        tpl[p] += flameCol;
                    }
                }
            }
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

// FXK7 - Prismatic Caustics
FxK7::FxK7() : LedEffect(fxk7Desc) {}

void FxK7::setup() {
    LedEffect::setup();
    phase1 = random16();
    phase2 = random16();
    phase3 = random16();
    phase4 = random16();
    colorBase = random8();
    tpl.fill_solid(BKG);
}

void FxK7::run() {
    EVERY_N_MILLIS_I(k7Timer, 25) {
        const uint16_t size = tpl.size();
        phase1 += 64;
        phase2 += 96;
        phase3 += 42;
        phase4 += 128;
        colorBase++;

        auto calcWave = [&](const int32_t x) -> uint8_t {
            const uint8_t w1 = sin8((x * 8) + (phase1 >> 8));
            const uint8_t w2 = sin8((x * 13) - (phase2 >> 8));
            const uint8_t w3 = sin8((x * 21) + (phase3 >> 8));
            const uint8_t w4 = sin8((x * 5) - (phase4 >> 8));
            const uint8_t s = (w1 + w2 + w3 + w4) / 4;
            // Non-linear cubic focal sharpness
            uint8_t c = scale8(s, s);
            c = scale8(c, s);
            return c;
        };

        for (uint16_t i = 0; i < size; ++i) {
            const uint8_t cCenter = calcWave(i);
            const uint8_t cRed = calcWave(i - 1);
            const uint8_t cBlue = calcWave(i + 1);

            // Base palette illumination with caustic focus
            const uint8_t cIdx = colorBase + (cCenter / 2);
            CRGB col = ColorFromPalette(palette, cIdx, cCenter, LINEARBLEND);

            // Prismatic chromatic dispersion (RGB diffraction at refractive focal points)
            col.r = qadd8(col.r, scale8(cRed, 80));
            col.g = qadd8(col.g, scale8(cCenter, 60));
            col.b = qadd8(col.b, scale8(cBlue, 110));

            // Brilliant focal peak flash
            if (cCenter > 210) {
                const uint8_t whiteHot = (cCenter - 210) * 4;
                col += CRGB(whiteHot, whiteHot, whiteHot);
            }

            tpl[i] = col;
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

// FXK8 - Neural Synapse
FxK8::FxK8() : LedEffect(fxk8Desc) {}

void FxK8::spawnSpike(const int32_t pos256, const int16_t vel256, const uint8_t colorIdx) {
    for (auto &sp : spikes) {
        if (!sp.active) {
            sp.pos256 = pos256;
            sp.vel256 = vel256;
            sp.colorIdx = colorIdx;
            sp.bri = 255;
            sp.active = true;
            return;
        }
    }
}

void FxK8::setup() {
    LedEffect::setup();
    const uint16_t size = tpl.size();
    activeSynapses = constrain(static_cast<uint8_t>(size / 14), static_cast<uint8_t>(3), maxSynapses);
    colorBase = random8();

    const uint16_t spacing = size / (activeSynapses + 1);
    for (uint8_t i = 0; i < activeSynapses; ++i) {
        synapses[i].pos = (i + 1) * spacing + (spacing > 3 ? random8(0, spacing / 3) : 0);
        if (synapses[i].pos >= size)
            synapses[i].pos = size - 1;
        synapses[i].activity = 0;
        synapses[i].refractory = 0;
    }

    for (auto &sp : spikes)
        sp.active = false;

    // Seed initial action potentials
    spawnSpike(0, 180, random8());
    if (size > 30)
        spawnSpike(static_cast<int32_t>(size - 1) << 8, -160, random8());

    tpl.fill_solid(BKG);
}

void FxK8::run() {
    EVERY_N_MILLIS_I(k8Timer, 25) {
        const uint16_t size = tpl.size();
        const int32_t maxPos256 = static_cast<int32_t>(size - 1) << 8;
        colorBase++;

        // Smooth synaptic trail decay
        tpl.nscale8(208);

        // Update synaptic nodes
        for (uint8_t i = 0; i < activeSynapses; ++i) {
            Synapse &syn = synapses[i];
            if (syn.activity > 0)
                syn.activity = scale8(syn.activity, 220);
            if (syn.refractory > 0)
                syn.refractory--;

            // Render synaptic node glow
            if (syn.pos < size) {
                const uint8_t restingHum = beatsin8(18, 15, 45);
                CRGB nodeCol = ColorFromPalette(palette, colorBase + (i * 32), restingHum, LINEARBLEND);
                if (syn.activity > 0) {
                    // Neurotransmitter burst flash
                    nodeCol += ColorFromPalette(palette, colorBase + 64, syn.activity, LINEARBLEND);
                    if (syn.activity > 180)
                        nodeCol += CRGB(syn.activity / 2, syn.activity / 2, syn.activity / 2);
                } else if (syn.refractory > 0) {
                    // Hyperpolarized refractory state
                    nodeCol += CRGB(0, 0, syn.refractory * 4);
                }
                tpl[syn.pos] += nodeCol;
            }
        }

        // Count active spikes
        uint8_t activeCount = 0;
        for (auto &sp : spikes) {
            if (!sp.active)
                continue;
            activeCount++;

            // Check interaction with synaptic nodes
            const uint16_t curPixel = sp.pos256 >> 8;
            for (uint8_t s = 0; s < activeSynapses; ++s) {
                Synapse &syn = synapses[s];
                if (curPixel == syn.pos && syn.refractory == 0) {
                    syn.activity = 255;
                    syn.refractory = 25;
                    // Synaptic branching: chance to trigger a reflected / daughter action potential
                    if (random8() < 120 && activeCount < maxSpikes) {
                        spawnSpike(sp.pos256, -sp.vel256, sp.colorIdx + 32);
                    }
                }
            }

            // Sub-pixel anti-aliased rendering of action potential head
            const auto idx = static_cast<uint16_t>(sp.pos256 >> 8);
            const auto frac = static_cast<uint8_t>(sp.pos256 & 0xFF);
            const CRGB col = ColorFromPalette(palette, colorBase + sp.colorIdx, sp.bri, LINEARBLEND) + CRGB(80, 80, 80);

            if (idx < size) {
                CRGB c = col; c.nscale8_video(255 - frac);
                tpl[idx] += c;
            }
            if (idx + 1 < size) {
                CRGB c = col; c.nscale8_video(frac);
                tpl[idx + 1] += c;
            }

            // Advance spike position
            sp.pos256 += sp.vel256;
            if (sp.pos256 < 0 || sp.pos256 > maxPos256) {
                sp.active = false;
            }
        }

        // Random spontaneous action potential generation at strand terminals or nodes
        if (activeCount < 3 && random8() < 40) {
            if (random8() < 128) {
                spawnSpike(0, random16(140, 260), random8());
            } else {
                spawnSpike(maxPos256, -static_cast<int16_t>(random16(140, 260)), random8());
            }
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}