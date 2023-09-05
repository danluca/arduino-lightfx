//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXE_H
#define LIGHTFX_FXE_H

#include "efx_setup.h"

namespace FxE {
    class FxE1 : public LedEffect {
    public:
        FxE1();

        void setup() override;

        void loop() override;

        const char *description() const override;

        static void twinkle();

        void describeConfig(JsonArray &json) const override;

        const char *name() const override;

        static void updateParams();
    };

    class FxE2 : public LedEffect {
    public:
        FxE2();

        void setup() override;

        void loop() override;

        const char *description() const override;

        void describeConfig(JsonArray &json) const override;

        const char *name() const override;

        void beatwave();
    };

    class FxE3 : public LedEffect {
    public:
        FxE3();

        void setup() override;

        void loop() override;

        const char *description() const override;

        const char *name() const override;

        void describeConfig(JsonArray &json) const override;
    protected:
        uint16_t segStart{}, segEnd{};
        uint16_t timerSlot{}, cycles{};
        Movement move = forward;
    };

    class FxE4 : public LedEffect {
    public:
        FxE4();

        void setup() override;

        void loop() override;

        const char *description() const override;

        const char *name() const override;

        void describeConfig(JsonArray &json) const override;

        void serendipitous();

    protected:
        uint16_t Xorig = 0x012;
        uint16_t Yorig = 0x015;
        uint16_t X, Y;
        uint8_t index;
    };
}

#endif //LIGHTFX_FXE_H
