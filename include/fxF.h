//
// Copyright 2023 by Dan Luca. All rights reserved
//
#ifndef ARDUINO_LIGHTFX_FXF_H
#define ARDUINO_LIGHTFX_FXF_H

#include "efx_setup.h"

namespace FxF {

    class FxF1 : public LedEffect {
    public:
        FxF1();

        void setup() override;

        void loop() override;

        const char *description() const override;

        const char *name() const override;

        void describeConfig(JsonArray &json) const override;
    };

    class FxF2 : public LedEffect {
    public:
        FxF2();

        void setup() override;

        void loop() override;

        const char *description() const override;

        const char *name() const override;

        void describeConfig(JsonArray &json) const override;

    protected:
        void makePattern(uint8_t hue);
        CRGBSet pattern;
    };
}
#endif //ARDUINO_LIGHTFX_FXF_H
