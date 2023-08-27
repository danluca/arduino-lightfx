// Copyright (c) 2023. by Dan Luca. All rights reserved.
//
#ifndef ARDUINO_LIGHTFX_PALETTEFACTORY_H
#define ARDUINO_LIGHTFX_PALETTEFACTORY_H

#include <FastLED.h>

enum Holiday {None, Party, Halloween, Thanksgiving, Christmas, NewYear};

static Holiday getHoliday(time_t time);

static Holiday currentHoliday();

Holiday parseHoliday(const String *str);

const char* holidayToString(Holiday hday);

class PaletteFactory {
    bool overrideHoliday = false;
    Holiday holiday = None;
public:
    CRGBPalette16 mainPalette(time_t time = 0);
    CRGBPalette16 secondaryPalette(time_t time = 0);
    void forceHoliday(Holiday hday);
    Holiday adjustHoliday(time_t time = 0);
    Holiday currentHoliday() const;
    static CRGBPalette16 randomPalette(uint8_t ofsHue = 0, time_t time = 0);
};

extern PaletteFactory paletteFactory;

#endif //ARDUINO_LIGHTFX_PALETTEFACTORY_H
