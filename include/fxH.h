//
// Copyright 2023,2024,2025 by Dan Luca. All rights reserved
//
#ifndef LIGHTFX_FXH_H
#define LIGHTFX_FXH_H

#include "efx_setup.h"
#include <vector>

namespace FxH {
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation,
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe speed of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100
#define COOLING 75

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 150
#define FRAMES_PER_SECOND 10

    class FxH1 : public LedEffect {
    private:
        static constexpr uint8_t numFires = 2;
        CRGBSet fires[numFires];
        std::vector<CRGB> hMap;
    public:
        FxH1();

        void setup() override;

        void run() override;

        void windDownPrep() override;

        void baseConfig(JsonObject &json) const override;

        void Fire2012WithPalette(uint8_t xFire);

        [[nodiscard]] uint8_t selectionWeight() const override;
    };

    class FxH2 : public LedEffect {
    public:
        FxH2();

        void setup() override;

        void run() override;

        void windDownPrep() override;

        void baseConfig(JsonObject &json) const override;

        static void confetti_pal();

        static void updateParams();

        [[nodiscard]] uint8_t selectionWeight() const override;
    };

    class FxH3 : public LedEffect {
    public:
        FxH3();

        void setup() override;

        void run() override;

        void windDownPrep() override;

        void baseConfig(JsonObject &json) const override;

        [[nodiscard]] uint8_t selectionWeight() const override;
    };

    class FxH4 : public LedEffect {
    public:
        FxH4();

        void setup() override;

        void run() override;

        void windDownPrep() override;

        void baseConfig(JsonObject &json) const override;

        [[nodiscard]] uint8_t selectionWeight() const override;
    private:
        static constexpr uint8_t twinkleDensity = 5;
        static constexpr uint8_t twinkleSpeed = 4;
        static constexpr uint8_t secondsPerPalette = 40;

        static void drawTwinkles(CRGBSet& set);
        static CRGB computeOneTwinkle(uint32_t ms, uint32_t salt);
        static uint8_t attackDecayWave8( uint8_t i);
        static void coolLikeIncandescent( CRGB& c, uint8_t phase);
    };

    class FxH5 : public LedEffect {
    public:
        FxH5();

        void setup() override;

        void run() override;

        bool windDown() override;

        void windDownPrep() override;

        void baseConfig(JsonObject &json) const override;

        [[nodiscard]] uint8_t selectionWeight() const override;
    private:
        int red {0};
        int green {0};
        int blue {255};

        int colorStep {0};
        int pixelPos {0};

        CRGB prevClr {};
        CRGBSet small, rest;
        uint8_t timer {0};
        enum FxState {Sparkle, RampUp, Glitter, RampDown} fxState {Sparkle};

        void electromagneticSpectrum(int transitionSpeed);
    };

    union Cycle {
        uint32_t compact;
        struct {
            uint8_t phase;
            uint8_t onTime;
            uint8_t offTime;
            uint8_t _reserved;
        };
        inline Cycle(const uint32_t compact) : compact(compact) { _reserved = 0; };
    };

    class Spark {
    public:
        explicit Spark(CRGB& ref);
        enum State:uint8_t {Idle, On, Off, WaitOn};
        State step(uint8_t dice);
        void on() const;
        void off() const;
        void reset();
        void activate(CRGB clr, Cycle cycle = 0);
        void setColor(CRGB clr);
    protected:
        State state;
        CRGB& pixel;
        CRGB fgClr, bgClr;
        bool dimBkg = false, loop = false;
        Cycle pattern, curCycle;

        friend class FxH6;  //intended to work closely with FxH6 effect
    };

    class FxH6 : public LedEffect {
    public:
        explicit FxH6();

        void setup() override;

        void run() override;

        bool windDown() override;

        void windDownPrep() override;

        [[nodiscard]] uint8_t selectionWeight() const override;

        ~FxH6() override;

    private:
        static constexpr int frameSize = 7;
        enum Phase:uint8_t {DefinedPattern, Random} stage;

        uint16_t timerCounter {};
        std::deque<Spark*> sparks {};
        std::deque<Spark*> activeSparks {};

        CRGBSet window, rest;

        void activateSparks(uint8_t howMany, uint8_t clrHint);
        void resetActivateAllSparks(uint8_t clrHint);
    };


}

#endif //LIGHTFX_FXH_H
