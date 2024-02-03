//
// Copyright 2023,2024 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXE_H
#define LIGHTFX_FXE_H

#include "efx_setup.h"

namespace FxE {
    class FxE1 : public LedEffect {
    public:
        FxE1();

        void setup() override;

        void run() override;

        bool windDown() override;

        static void twinkle();

        JsonObject & describeConfig(JsonArray &json) const override;

        static void updateParams();

        uint8_t selectionWeight() const override;
    };

    class FxE2 : public LedEffect {
    public:
        FxE2();

        void setup() override;

        void run() override;

        void windDownPrep() override;

        void beatwave();

        uint8_t selectionWeight() const override;
    };

    class FxE3 : public LedEffect {
    public:
        FxE3();

        void setup() override;

        void run() override;

        void windDownPrep() override;

        uint8_t selectionWeight() const override;

    protected:
        const uint8_t sasquatchSize = 3;
        enum Movement {forward, backward, sasquatch, pauseF, pauseB};
        CRGBSet shdOverlay;
        uint16_t segStart{}, segEnd{};
        uint16_t timerSlot{}, cycles{};
        Movement move = forward;
    };

    class FxE4 : public LedEffect {
    public:
        FxE4();

        void setup() override;

        void run() override;

        void windDownPrep() override;

        void serendipitous();

        uint8_t selectionWeight() const override;

    protected:
        uint16_t Xorig = 0x012;
        uint16_t Yorig = 0x015;
        uint16_t X{}, Y{};
        uint8_t index{};
    };

    class FxE5 : public LedEffect {
    public:
        FxE5();

        void setup() override;

        void run() override;

        void windDownPrep() override;

        uint8_t selectionWeight() const override;

    protected:
        CRGBSet wave2, wave3;
        uint8_t clr1, clr2, clr3;
        uint16_t pos2, pos3;
        const uint8_t segSize = 8;
    };
}

#endif //LIGHTFX_FXE_H
