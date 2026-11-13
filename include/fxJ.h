//
// Copyright by Dan Luca. All rights reserved
//
#ifndef ARDUINO_LIGHTFX_FXJ_H
#define ARDUINO_LIGHTFX_FXJ_H

#include "efx_setup.h"

namespace FxJ {
    // FXJ1: Popcorn - kernels sit near the floor (index 0), wiggle as they heat up, then pop, fly up and fall back under gravity
    // Ported from WS2812FX custom effect Popcorn.h by Keith Lord (MIT license)
    class FxJ1 : public LedEffect {
    public:
        FxJ1();
        void setup() override;
        void run() override;

    private:
        static constexpr uint8_t maxKernels = 10;
        struct Kernel {
            int32_t pos256;     // position * 256; negative means inactive
            int32_t vel256;     // velocity * 256 per tick
            int32_t rest256;    // resting spot near the floor while warming up, * 256
            uint8_t colorIdx;   // palette color index
            uint8_t warm;       // ticks left wiggling before the pop; 0 when not warming
            uint8_t warmTotal;  // total warm-up ticks, for progress
            uint8_t phase;      // wiggle oscillation phase
        };
        void heat(Kernel& k);
        void pop(Kernel& k);

        Kernel kernels[maxKernels]{};
        int32_t maxVel256{};    // launch velocity ceiling, scaled to the frame size
        int32_t maxRest256{};   // how far from the floor kernels may rest while warming, * 256
        CRGB bgColor;
        uint8_t popChance{};    // chance (out of 256) per tick an inactive kernel pops
        uint8_t flash{};        // brief brightening of the pixel a kernel just popped from
        uint16_t flashIdx{};
        bool dirRight{};
    };

    // FXJ2: Rain on a window - drops hit the glass with a splash, cling for a while, then slide down (index 0 is the floor)
    // in a stick-slip fashion, merging with drops they catch and leaving wet trails that slowly dry
    // Inspired by WS2812FX custom effect Rain.h by Keith Lord (MIT license)
    class FxJ2 : public LedEffect {
    public:
        FxJ2();
        void setup() override;
        void run() override;

    private:
        static constexpr uint8_t maxDrops = 12;
        static constexpr uint8_t maxMass = 6;
        struct Drop {
            int32_t pos256;     // position * 256; negative means inactive
            int32_t vel256;     // downward speed * 256 per tick
            uint8_t mass;       // drop size - heavier drops slide faster and brighter; mass 1 droplets never slide
            uint8_t colorIdx;   // palette color index
            uint16_t hold;      // ticks left clinging to the glass before sliding
            uint8_t bri;        // brightness - fades as a droplet evaporates
            uint8_t flash;      // impact splash brightness
        };
        void spawn(int32_t pos256, uint8_t mass, bool splash);
        void merge();

        Drop drops[maxDrops]{};
        CRGB bgColor;
        uint8_t bpm{};                  // rain intensity cycle - drizzle to shower
        uint8_t tick{};
        bool dirRight{};
        ScratchView<uint8_t> wetTrail{};
        ScratchView<uint8_t> wetTrailColors{};
    };

    // FXJ3: 1D Wave Equation - physical wave propagation, reflection, damping and interference along the strand
    class FxJ3 : public LedEffect {
    public:
        FxJ3();
        void setup() override;
        void run() override;

    private:
        ScratchView<uint8_t> heightView{};
        ScratchView<uint8_t> velView{};
        uint8_t dropChance{};
        uint8_t colorBase{};
        CRGB bgColor;
        void injectDrop();
    };

    // FXJ4: Meteor Shower with Thermal Sparks - sub-pixel anti-aliased meteors shedding decaying heat trails and micro-sparks
    class FxJ4 : public LedEffect {
    public:
        FxJ4();
        void setup() override;
        void run() override;

    private:
        static constexpr uint8_t maxMeteors = 6;
        static constexpr uint8_t maxSparks = 16;

        struct Meteor {
            int32_t pos256;      // position * 256; negative means inactive
            int32_t vel256;      // velocity * 256 per tick
            uint8_t colorIdx;    // palette index
            uint8_t trailHeat;   // heat injected into trail
            uint8_t decay;       // trail decay speed
        };

        struct Spark {
            int32_t pos256;      // position * 256; negative means inactive
            int32_t vel256;      // velocity * 256
            uint8_t colorIdx;
            uint8_t bri;         // current brightness
            uint8_t decay;       // fade rate per tick
        };

        void spawnMeteor();
        void spawnSpark(int32_t pos256, int32_t vel256, uint8_t colorIdx);

        Meteor meteors[maxMeteors]{};
        Spark sparks[maxSparks]{};
        ScratchView<uint8_t> heatTrail{};
        ScratchView<uint8_t> heatColors{};
        CRGB bgColor;
        uint8_t spawnChance{};
        uint16_t spawnCount{};
        bool dirRight{};
    };
}
#endif //ARDUINO_LIGHTFX_FXJ_H
