//
// Copyright 2023,2024 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXA_H
#define LIGHTFX_FXA_H

#include "efx_setup.h"

namespace FxA {
    uint16_t fxa_incStackSize(int16_t delta, uint16_t max);
    uint16_t fxa_stackAdjust(CRGBSet &set, uint16_t szStackSeg);
    void resetStack();

    class FxA1 : public LedEffect {
    public:
        FxA1();

        void setup() override;

        void run() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;

    protected:
        void makeDot(CRGB color, uint16_t szDot);

        CRGBSet dot;
        uint8_t szSegment = 3;
        uint8_t szStackSeg = 2;
    };

    class FxA2 : public LedEffect {
    public:
        FxA2();

        void setup() override;

        void run() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;

    protected:
        enum Movement { forward, pause, backward };
        void makeDot();
        CRGBSet dot;
        uint8_t szSegment = 5;
        Movement movement = forward;
        uint8_t spacing = 1;
    };

    class FxA3 : public LedEffect {
    public:
        FxA3();

        void setup() override;

        void run() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;

    protected:
        CRGBSet dot;

        void makeDot(CRGB color, uint16_t szDot);

        uint8_t szSegment = 5;
        //uint8_t szStackSeg = 3;
        bool bFwd = true;
    };

    class FxA4 : public LedEffect {
    public:
        FxA4();

        void setup() override;

        void run() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;

    protected:
        CRGBSet dot;
        CRGBSet frL;
        CRGBSet frR;

        void makeDot(CRGB color, uint16_t szDot);

        uint8_t szSegment = 5;
        uint8_t szStackSeg = 3;
        uint8_t spacing = 7;
        CRGB curBkg;
    };

    class FxA5 : public LedEffect {
    public:
        FxA5();

        void setup() override;

        void run() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;

    protected:
        CRGBSet ovr;

        void makeFrame();
    };
}
#endif //LIGHTFX_FXA_H
