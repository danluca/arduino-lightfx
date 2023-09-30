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

    class EyeBlink {
    public:
        EyeBlink();

    protected:
        uint16_t idleTime;  //don't reuse this immediately
        uint8_t curBrightness; //current brightness
        uint8_t brIncr; //brightness increment - determines blinking speed
        uint8_t curLen; //current eye size
        uint8_t pauseTime;  //idle time between blinks
        uint8_t curPause;   //current pause counter
        uint8_t numBlinks;  //how many times to blink before deactivate,
        uint16_t pos;   //eye's position in the parent set
        CRGB color;    //current color

        static const uint8_t eyeGapSize = 2;
        static const uint8_t padding = 3;
        static const uint8_t eyeSize = 3;   // must be odd number, iris size is implied 1
        static const uint8_t size {eyeSize*2+eyeGapSize+padding};
        enum BlinkSteps {OpenLid, PauseOn, CloseLid, PauseOff, Idle, Off};
        BlinkSteps curStep;
        CRGBSet * const holderSet;

        void start();
        void step();
        void reset(uint16_t curPos, CRGB clr);
        bool isActive() const;
        inline operator bool() { return isActive(); }

        friend class FxF3;  //intended for FxF3 to have deeper access
    };

    class FxF3 : public LedEffect {
    public:
        FxF3();

        void setup() override;

        void loop() override;

        const char *description() const override;

        const char *name() const override;

        void describeConfig(JsonArray &json) const override;

        Viewport nextEyePos();

        EyeBlink *findAvailableEye();

    protected:
        static const uint8_t maxEyes = 5;   //correlated with size of a FRAME
        EyeBlink eyes[maxEyes]{};
    };

    class FxF4 : public LedEffect {
    public:
        FxF4();

        void setup() override;

        void loop() override;

        const char *description() const override;

        const char *name() const override;

        void describeConfig(JsonArray &json) const override;
    };
}
#endif //ARDUINO_LIGHTFX_FXF_H
