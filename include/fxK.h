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
        static constexpr uint8_t maxRules = 4;
        static const uint8_t rules[maxRules];
        uint8_t currentRuleIdx{};
        uint16_t genCount{};
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
            uint8_t colorIdx;
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
}
#endif //ARDUINO_LIGHTFX_FXK_H
