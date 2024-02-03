//
// Copyright 2023,2024 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXB_H
#define LIGHTFX_FXB_H

#include "efx_setup.h"
//typedef void (*SimplePatternList[])();  // List of patterns to cycle through.  Each is defined as a separate function below.

namespace FxB {
    void fadein();

    void rainbow();

    void rainbowWithGlitter();

    void addGlitter(fract8 chanceOfGlitter);

    void fxb_confetti();

    void sinelon();

    void bpm();

    void juggle_short();

    void juggle_long();

    void ease();


    class FxB1 : public LedEffect {
    public:
        FxB1();

        void setup() override;

        void run() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;
    };

    class FxB2 : public LedEffect {
    public:
        FxB2();

        void setup() override;

        void run() override;

        uint8_t selectionWeight() const override;
    };

    class FxB3 : public LedEffect {
    public:
        FxB3();

        void setup() override;

        void run() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;
    };

    class FxB4 : public LedEffect {
    public:
        FxB4();

        void setup() override;

        void run() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;
    };

    class FxB5 : public LedEffect {
    public:
        FxB5();

        void setup() override;

        void run() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;
    };

    class FxB6 : public LedEffect {
    public:
        FxB6();

        void setup() override;

        void run() override;

        uint8_t selectionWeight() const override;
    };

    class FxB7 : public LedEffect {
    public:
        FxB7();

        void setup() override;

        void run() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;
    };

    class FxB8 : public LedEffect {
    public:
        FxB8();

        void setup() override;

        void run() override;

        JsonObject & describeConfig(JsonArray &json) const override;

        uint8_t selectionWeight() const override;
    };

    class FxB9 : public LedEffect {
    public:
        FxB9();

        void setup() override;

        void run() override;

        uint8_t selectionWeight() const override;
    };
}

#endif //LIGHTFX_FXB_H
