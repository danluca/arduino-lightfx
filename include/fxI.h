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
void fxRegister(); // Function to register the new effect

    class FxI1 : public LedEffect {
    public:
        FxI1(); // Constructor
        void setup() override;
        void run() override; // Main loop for the ping-pong effect
        void reWall();

    private:
        uint16_t wallStart;
        uint16_t wallEnd;
        uint16_t currentPos;
        uint16_t segmentLength;
        bool forward;
        uint8_t fgColor, bgColor;
        uint16_t speed;
    };

}
#endif //ARDUINO_LIGHTFX_FXI_H
