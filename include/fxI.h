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

}
#endif //ARDUINO_LIGHTFX_FXI_H
