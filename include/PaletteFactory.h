//
// Copyright (c) 2023. by Dan Luca. All rights reserved.
//
#ifndef ARDUINO_LIGHTFX_PALETTEFACTORY_H
#define ARDUINO_LIGHTFX_PALETTEFACTORY_H

#include "util.h"
#include <FastLED.h>

namespace colTheme {
    class PaletteFactory {
        bool autoChangeHoliday = true;
        Holiday holiday = None;
    public:
        CRGBPalette16 mainPalette(time_t time = 0);

        CRGBPalette16 secondaryPalette(time_t time = 0);

        void setHoliday(Holiday hday);

        Holiday adjustHoliday(time_t time = 0);

        Holiday getHoliday() const;

        bool isHolidayLimitedHue() const;

        bool isAuto() const;

        void setAuto(bool autoMode = true);

        static CRGBPalette16 randomPalette(uint8_t ofsHue = 0, time_t time = 0);
    };
}

extern colTheme::PaletteFactory paletteFactory;

#endif //ARDUINO_LIGHTFX_PALETTEFACTORY_H
