//
// Copyright by Dan Luca. All rights reserved
//
#ifndef ARDUINO_LIGHTFX_FXJ_H
#define ARDUINO_LIGHTFX_FXJ_H

#include "efx_setup.h"

namespace FxJ {
    // FXJ1: Popcorn - kernels randomly pop from the floor (index 0), fly up and fall back under gravity
    // Ported from WS2812FX custom effect Popcorn.h by Keith Lord (MIT license)
    class FxJ1 : public LedEffect {
    public:
        FxJ1();
        void setup() override;
        void run() override;
        [[nodiscard]] uint8_t selectionWeight() const override;

    private:
        static constexpr uint8_t maxKernels = 10;
        struct Kernel {
            int32_t pos256;     // position * 256; negative means inactive
            int32_t vel256;     // velocity * 256 per tick
            uint8_t colorIdx;   // palette color index
        };
        void pop(Kernel& k);

        Kernel kernels[maxKernels]{};
        int32_t maxVel256{};    // launch velocity ceiling, scaled to the frame size
        CRGB bgColor;
        uint8_t popChance{};    // chance (out of 256) per tick an inactive kernel pops
        uint8_t flash{};        // brief brightening of the floor pixel on each pop
        bool dirRight{};
    };
}
#endif //ARDUINO_LIGHTFX_FXJ_H
