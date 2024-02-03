//
// Copyright 2023,2024 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXD_H
#define LIGHTFX_FXD_H

#include "efx_setup.h"

namespace FxD {
    class FxD1 : public LedEffect {
    public:
        FxD1();

        void setup() override;

        void run() override;

        bool windDown() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        void ChangeMe();

        void confetti();

        uint8_t selectionWeight() const override;
    };

    class FxD2 : public LedEffect {
    public:
        FxD2();

        void setup() override;

        void run() override;

        bool windDown() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        void dot_beat();

        uint8_t selectionWeight() const override;
    };

    class FxD3 : public LedEffect {
    public:
        FxD3();

        void setup() override;

        void run() override;

        void windDownPrep() override;

        void plasma();

        uint8_t selectionWeight() const override;

    protected:
        uint8_t monoColor;
    };

    class FxD4 : public LedEffect {
    public:
        FxD4();

        void setup() override;

        void run() override;

        bool windDown() override;

        void rainbow_march();

        void update_params(uint8_t slot);

        uint8_t selectionWeight() const override;
    };


    struct ripple {
        uint8_t rpBright;
        uint8_t color;
        uint16_t center;
        uint8_t rpFade;   // low value - slow fade to black
        uint8_t step;
        CRGBSet *pSeg;

        void Move();
        void Fade() const;
        bool Alive() const;
        void Init(CRGBSet *set);

    };

    typedef struct ripple Ripple;

    class FxD5 : public LedEffect {
    public:
        FxD5();

        void setup() override;

        void run() override;

        void windDownPrep() override;

        void ripples();

        uint8_t selectionWeight() const override;

    protected:
        static const uint8_t maxRipples = 8;
        Ripple ripplesData[maxRipples]{};


    };
}

#endif //LIGHTFX_FXD_H
