//
// Copyright by Dan Luca. All rights reserved
//
#ifndef ARDUINO_LIGHTFX_FXK_H
#define ARDUINO_LIGHTFX_FXK_H

#include "efx_setup.h"

namespace FxK {
    // FXK1: Aurora Borealis - multi-harmonic flowing simplex noise curtains with polar aurora palettes
    class FxK1 : public LedEffect {
    public:
        FxK1();
        void setup() override;
        void run() override;

    private:
        uint16_t noiseScale1{};
        uint16_t noiseScale2{};
        uint16_t noiseScale3{};
        uint32_t xOffset1{};
        uint32_t xOffset2{};
        uint32_t xOffset3{};
        uint8_t breathPhase{};
        uint8_t colorBase{};
    };

    // FXK2: 1D Cellular Automata - evolving Wolfram elementary cellular automata with color-mapped cell lifespans
    class FxK2 : public LedEffect {
    public:
        FxK2();
        void setup() override;
        void run() override;

    private:
        static constexpr uint8_t maxRules = 6;
        static const uint8_t rules[maxRules];
        uint8_t currentRuleIdx{};
        uint16_t genCount{};
        uint8_t stepTimer{};
        uint8_t colorBase{};
        ScratchView<uint8_t> cellState{};
        ScratchView<uint8_t> cellAge{};
        void seedAutomaton();
        void stepRule();
    };

    // FXK3: Organic Heartbeat - anatomical dual-thump (lub-dub) cardiac cycle with radial vascular shockwaves
    class FxK3 : public LedEffect {
    public:
        FxK3();
        void setup() override;
        void run() override;

    private:
        enum CardiacPhase : uint8_t { Diastole, SystoleS1, Pause, SystoleS2 };
        CardiacPhase phase{};
        uint16_t phaseTimer{};
        uint16_t wavePos1_256{};
        uint16_t wavePos2_256{};
        uint8_t waveBri1{};
        uint8_t waveBri2{};
        uint8_t centerGlow{};
        uint8_t colorBase{};
        int32_t pulseOrigin256{};
        int32_t targetOrigin256{};
        bool s2Active{};
    };

    // FXK4: N-Body Gravitational Attractors - virtual mass bodies pulling colored particle swarms into gravitational orbits
    class FxK4 : public LedEffect {
    public:
        FxK4();
        void setup() override;
        void run() override;

    private:
        static constexpr uint8_t numAttractors = 3;
        static constexpr uint8_t numParticles = 16;

        struct Attractor {
            int32_t pos256;      // position * 256
            int32_t vel256;      // velocity * 256 per tick
            int32_t mass;        // gravitational mass
            uint8_t colorOffset;
        };

        struct Particle {
            int32_t pos256;      // position * 256
            int32_t vel256;      // velocity * 256 per tick
            uint8_t colorIdx;
            uint8_t bri;
        };

        Attractor attractors[numAttractors]{};
        Particle particles[numParticles]{};
        CRGB bgColor;
        uint8_t colorBase{};
    };

    // FXK5: Thermal Diffusion / Heat Shimmer - 1D heat equation with conductive diffusion, convective draft shimmer, and thermal radiation
    class FxK5 : public LedEffect {
    public:
        FxK5();
        void setup() override;
        void run() override;

    private:
        ScratchView<uint8_t> heat{};
        ScratchView<uint8_t> heatNext{};
        uint16_t noisePos{};
        uint8_t coolingRate{};
        uint8_t diffusionRate{};
        uint8_t colorBase{};
        void injectHeat();
    };

    // FXK6: Distributed Candle / Multi-Flame Cluster - independent candles with varied initial heights, progressive burn-down rates, and mini-Fire2012 convective flames
    class FxK6 : public LedEffect {
    public:
        FxK6();
        void setup() override;
        void run() override;

    private:
        static constexpr uint8_t maxCandles = 8;
        static constexpr uint8_t flameCells = 4;

        struct Candle {
            uint16_t basePos;       // base pixel index on strip
            uint8_t maxHeight;      // starting max wax height in pixels
            uint16_t height256;     // current wax height * 256
            uint8_t burnRate;       // consumption rate per burn step
            uint8_t flameHeat[flameCells]; // convective flame heat map (FXH1 style)
            uint16_t draftPhase;    // air draft turbulence phase
            uint8_t flareCountdown; // re-ignition flare timer
        };

        Candle candles[maxCandles]{};
        uint8_t activeCandles{};
        uint8_t colorBase{};
        void initCandle(Candle &c, uint16_t base, uint8_t maxH);
    };

    // FXK7: Prismatic Caustics - refracted light web with constructive focal peaks and prismatic chromatic dispersion
    class FxK7 : public LedEffect {
    public:
        FxK7();
        void setup() override;
        void run() override;

    private:
        uint16_t phase1{};
        uint16_t phase2{};
        uint16_t phase3{};
        uint16_t phase4{};
        uint8_t colorBase{};
    };

    // FXK8: Neural Synapse - bio-electric action potentials propagating across synaptic nodes with refractory recovery
    class FxK8 : public LedEffect {
    public:
        FxK8();
        void setup() override;
        void run() override;

    private:
        static constexpr uint8_t maxSynapses = 8;
        static constexpr uint8_t maxSpikes = 12;

        struct Synapse {
            uint16_t pos;
            uint8_t activity;    // synaptic neurotransmitter release glow
            uint8_t refractory;  // post-firing hyperpolarization timer
        };

        struct Spike {
            int32_t pos256;      // position * 256
            int16_t vel256;      // velocity * 256
            uint8_t colorIdx;
            uint8_t bri;
            bool active;
        };

        Synapse synapses[maxSynapses]{};
        Spike spikes[maxSpikes]{};
        uint8_t activeSynapses{};
        uint8_t colorBase{};
        void spawnSpike(int32_t pos256, int16_t vel256, uint8_t colorIdx);
    };
}
#endif //ARDUINO_LIGHTFX_FXK_H
