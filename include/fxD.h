//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXD_H
#define LIGHTFX_FXD_H

#include "efx_setup.h"

namespace FxD {
    class FxD1 : public LedEffect {
    public:
        FxD1();

        void setup() override;

        void loop() override;

        const char *description() const override;

        void describeConfig(JsonArray &json) const override;

        const char *name() const override;

        void ChangeMe();

        void confetti();
    };

    class FxD2 : public LedEffect {
    public:
        FxD2();

        void setup() override;

        void loop() override;

        void describeConfig(JsonArray &json) const override;

        const char *name() const override;

        const char *description() const override;

        void dot_beat();
    };

    class FxD3 : public LedEffect {
    public:
        FxD3();

        void setup() override;

        void loop() override;

        void describeConfig(JsonArray &json) const override;

        const char *description() const override;

        const char *name() const override;

        void plasma();
    };

    class FxD4 : public LedEffect {
    public:
        FxD4();

        void setup() override;

        void loop() override;

        const char *description() const override;

        const char *name() const override;

        void describeConfig(JsonArray &json) const override;

        void rainbow_march();

        void update_params(uint8_t slot);
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

        void loop() override;

        const char *description() const override;

        const char *name() const override;

        void describeConfig(JsonArray &json) const override;

        void ripples();

    protected:
        static const uint8_t maxRipples = 8;
        Ripple ripplesData[maxRipples]{};


    };
}

#endif //LIGHTFX_FXD_H
