//
// Copyright 2023,2024,2025 by Dan Luca. All rights reserved
//
#ifndef ARDUINO_LIGHTFX_FXI_H
#define ARDUINO_LIGHTFX_FXI_H

#include "efx_setup.h"
#include <vector>
// defines needed before including FHT
#define LOG_OUT 1       // use the log output function
#define FHT_N   32      //32 point FHT
#include <FHT.h>

namespace FxI {
    class FxI1 : public LedEffect {
    public:
        FxI1(); // Constructor
        void setup() override;
        void run() override; // Main loop for the ping-pong effect
        void reWall();
        [[nodiscard]] uint8_t selectionWeight() const override;

    private:
        uint16_t wallStart, wallEnd, prevWallStart, prevWallEnd;
        uint16_t currentPos;
        int16_t midPointOfs;
        uint16_t segmentLength;
        bool forward;
        uint8_t fgColor, bgColor;
    };

    class FxI2 : public LedEffect {
    public:
        FxI2();
        void setup() override;
        void run() override;
        [[nodiscard]] inline uint8_t selectionWeight() const override;

    private:
        void pacifica_loop();
        void pacifica_one_layer(const CRGBPalette16& p, uint16_t ciStart, uint16_t waveScale, uint8_t bri, uint16_t ioff);
        void pacifica_add_whitecaps();
        void pacifica_deepen_colors();

        uint16_t sCIStart1{}, sCIStart2{}, sCIStart3{}, sCIStart4{};
        uint32_t sLastMs = 0;
    };

    class FxI3 : public LedEffect {
    public:
        FxI3();
        void setup() override;
        void init_drop(int32_t maxPos);
        void run() override;
        [[nodiscard]] uint8_t selectionWeight() const override;
    private:
        // Fixed-point physics (8 fractional bits)
        int32_t pos256{};   // position * 256
        int32_t vel256{};   // velocity * 256 per tick
        int16_t gravity{};  // gravity per tick (positive pulls to the right by convention)
        uint8_t hueIdx{};   // color index for palette
        uint8_t trail{};    // glow radius
        uint8_t fadeAmt{};  // trail fade per frame
        uint8_t loss{};     // bounce energy loss (0..255), e.g., 200 = ~78%
        bool dirRight{};    // initial direction
        uint8_t sparkTicks{}; // brief flash on bounce
        // drop/bounce lifecycle
        bool settled{};           // true when ball has come to rest on the floor
        uint16_t restHold{};      // frames to hold the settled state before restarting
    };

    // FXI4: Audio-seeded VU segments
    class FxI4 final : public LedEffect {
    public:
        FxI4();
        void setup() override;
        void run() override;
        [[nodiscard]] uint8_t selectionWeight() const override;

    private:
        void loadSeedFromFile();
        void drawSegments();
        uint8_t levelFromSeed(uint8_t band);

        // configuration
        // text files with integers [0..255] separated by comma/space/newline
        static constexpr auto seedFiles_prefix = "/ext/fx/fxi4_seed";
        static constexpr auto seedFile_extension = ".txt";
        uint8_t segments = 8;                      // number of VU segments across the strip
        uint8_t smoothing = 196;                    // IIR smoothing factor (lower = smoother)
        uint16_t segmentGap = 1;                   // pixels gap between segments
        uint16_t frameMs = 60;                     // update interval in milliseconds (~25 FPS)
        uint16_t peakHoldMs = 250;                 // peak hold duration per segment

        // runtime data
        std::vector<uint8_t> seed{};                 // seed data 0..255; either mono envelope or frames×bands (row-major)
        size_t seedPos = 0;                         // legacy mono position
        size_t framePos = 0;                        // frame index for frames×bands
        size_t frames = 0;                          // number of frames when seed is matrix-like
        bool seedHasBands = false;                  // true when seed.size() % segments == 0
        std::vector<uint8_t> hist{};                  // recent levels, size == segments
        std::vector<uint8_t> peaks{};                 // recent peak levels per segment
        std::vector<uint32_t> peakTs{};              // last time peak updated per segment
        uint8_t baseHue = 0;                       // base hue offset to drift colors
    };

    // FXI5: Shoreline waves with whitecaps and backwash
    class FxI5 final : public LedEffect {
    public:
        FxI5();
        void setup() override;
        void run() override;
        [[nodiscard]] uint8_t selectionWeight() const override;

    private:
        // main swell moving towards shore (index 0)
        uint16_t swellPos = 0;           // current center position of approaching wave
        uint16_t swellWidth = 12;        // visual width of the crest
        bool swellCrashed = false;       // true while the swell is breaking at the shoreline
        uint8_t crashHold = 0;           // short hold to emulate the breaking crest

        // foam at shore upon impact
        uint8_t foamLevel = 0;           // 0..255 fade for whitecaps at shore
        uint32_t lastTick = 0;           // timing accumulator

        // brief backwash after impact: a dim wave receding from shore
        bool backwashActive = false;
        uint16_t backwashPos = 0;        // center of backwash wave
        uint16_t backwashWidth = 9;
        uint16_t backwashPeriodMs = 28;

        // palette motion
        uint8_t seaHueBase = 0;          // base index for sea color from targetPalette

        // "sea shore" size
        static constexpr uint8_t frameSize = 40;
        CRGBSet frame;
        CRGBSet rest;

        // Beach section near the shore (index 0)
        static constexpr uint8_t kBeachMin = 5;
        static constexpr uint8_t kBeachMax = 10;   // request: 5-10 pixels
        uint8_t beachLen = kBeachMin;              // actual beach length used
        uint8_t beachDryRate = 3;                  // per-frame dry decay
        uint8_t beachWetBoost = 120;               // wetness added on impact (peak)
        uint8_t beachWashDepth = 6;                // how deep the wave wets the beach (clamped to beachLen)
        uint8_t beachHueDryOfs = 40;               // palette hue offset for dry sand
        uint8_t beachHueWetOfs = 20;               // palette hue offset for wet sand
        uint8_t beachBriDry = 26;                  // brightness for dry sand
        uint8_t beachBriWet = 40;                  // brightness for wet sand
        uint8_t beachWet[ kBeachMax ]{};           // per-pixel wetness 0..255

        void drawSeaBackground();
        void drawSwell(uint16_t center, uint16_t width, uint8_t crestBri, int8_t dirSign);
        void drawFoamAtShore(uint8_t level);
        void drawBeach();
        void updateBeachWetness(bool impactNow);
        void drawSplash(uint8_t intensity); // brief spray at the shoreline when crashing
    };

}
#endif //ARDUINO_LIGHTFX_FXI_H
