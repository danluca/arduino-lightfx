//
// Copyright 2023,2024 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXC_H
#define LIGHTFX_FXC_H

#include "efx_setup.h"

namespace FxC {
    class FxC1 : public LedEffect {
    private:
        CRGBSet setA, setB;

    public:
        FxC1();

        void setup() override;

        void run() override;

        bool windDown() override;

        void animationA();

        void animationB();

        uint8_t selectionWeight() const override;
    };

    class FxC2 : public LedEffect {
    public:
        FxC2();

        //void setup() override;

        void run() override;

        void windDownPrep() override;

        uint8_t selectionWeight() const override;
    };

    class FxC3 : public LedEffect {
    public:
        FxC3();

        void setup() override;

        void run() override;

        bool windDown() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;
    };

    class FxC4 : public LedEffect {
    public:
        FxC4();

        void setup() override;

        void run() override;

        bool windDown() override;

        uint8_t selectionWeight() const override;

    protected:
        uint8_t frequency {10};
        uint8_t flashes {12};
    };

    class FxC5 : public LedEffect {
    public:
        FxC5();

        void setup() override;

        void run() override;

        bool windDown() override;

        void changeParams();

        void matrix();

        uint8_t selectionWeight() const override;

    protected:
        uint8_t palIndex = 95;
        bool hueRot = false;                                     // Does the hue rotate? 1 = yes
        uint8_t bgClr = 0;
        uint8_t bgBri = 0;
    };

    class FxC6 : public LedEffect {
    public:
        FxC6();

        void setup() override;

        void run() override;

        bool windDown() override;

        void one_sine_pal(uint8_t colorIndex);

        uint8_t selectionWeight() const override;

    protected:
        uint8_t allfreq = 32;                                     // You can change the frequency, thus distance between bars.
        int phase = 0;                                            // Phase change value gets calculated.
        uint8_t cutoff = 192;                                     // You can change the cutoff value to display this wave. Lower value = longer wave.
        int delay = 30;                                           // You can change the delay. Also you can change the speed global variable.
        uint8_t bgclr = 0;                                        // A rotating background colour.
        uint8_t bgbright = 10;                                    // Brightness of background colour

    };
}
#endif //LIGHTFX_FXC_H
