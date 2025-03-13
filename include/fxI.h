//
// Copyright 2023,2024,2025 by Dan Luca. All rights reserved
//
#ifndef ARDUINO_LIGHTFX_FXI_H
#define ARDUINO_LIGHTFX_FXI_H

#include "efx_setup.h"
// defines needed before including FHT
#define LOG_OUT 1       // use the log output function
#define FHT_N   32      //32 point FHT
#include <FHT.h>

namespace FxI {
    class FxI1 : public LedEffect {
    public:
        FxI1(); // Constructor
        void setup() override;
        void run() override; // Main loop for the ping-pong effect
        void reWall();
        [[nodiscard]] uint8_t selectionWeight() const override;

    private:
        uint16_t wallStart, wallEnd, prevWallStart, prevWallEnd;
        uint16_t currentPos;
        int16_t midPointOfs;
        uint16_t segmentLength;
        bool forward;
        uint8_t fgColor, bgColor;
    };

    class FxI2 : public LedEffect {
    public:
        FxI2();
        void setup() override;
        void run() override;
        [[nodiscard]] inline uint8_t selectionWeight() const override;

    private:
        void pacifica_loop();
        void pacifica_one_layer(const CRGBPalette16& p, uint16_t ciStart, uint16_t waveScale, uint8_t bri, uint16_t ioff);
        void pacifica_add_whitecaps();
        void pacifica_deepen_colors();

        uint16_t sCIStart1{}, sCIStart2{}, sCIStart3{}, sCIStart4{};
        uint32_t sLastMs = 0;
    };

}
#endif //ARDUINO_LIGHTFX_FXI_H
