// Copyright (c) 2023 by Dan Luca. All rights reserved.
//

#include "PaletteFactory.h"
#include <TimeLib.h>

using namespace colTheme;
/// Other custom palette definitions
/**
 * Halloween themed colors - red, orange, purple and black
 */
extern const TProgmemRGBPalette16 HalloweenColors_p FL_PROGMEM = {
        CRGB::DarkRed, CRGB::DarkRed, CRGB::Red, CRGB::Red,
        CRGB::DarkOrange, CRGB::DarkOrange, CRGB::OrangeRed, CRGB::OrangeRed,
        CRGB::Orange, CRGB::Orange, CRGB::Purple, CRGB::Purple,
        CRGB::Purple, CRGB::Black, CRGB::Black, CRGB::Black
};
extern const TProgmemRGBPalette16 HalloweenStripeColors_p FL_PROGMEM = {
        CRGB::DarkRed, CRGB::Black, CRGB::Red, CRGB::Black,
        CRGB::DarkOrange, CRGB::Black, CRGB::OrangeRed, CRGB::Black,
        CRGB::Orange, CRGB::Black, CRGB::Purple, CRGB::Black,
        CRGB::DarkGray, CRGB::Black, CRGB::Gray, CRGB::Black
};
/**
 * Christmas colors - red, green, white
 */
extern const TProgmemRGBPalette16 ChristmasColors_p FL_PROGMEM = {
        CRGB::DarkGreen, CRGB::DarkGreen, CRGB::Green, CRGB::Green,
        CRGB::DarkRed, CRGB::DarkRed, CRGB::Red, CRGB::Red,
        CRGB::Gray, CRGB::Gray, CRGB::White, CRGB::White,
        CRGB::DarkGray, CRGB::Black, CRGB::Gray, CRGB::Black
};
/**
 * Partier colors - a derivation on built-in party colors palette
 */
 extern const TProgmemPalette16 PartierColors_p FL_PROGMEM = {
        0x5500AB, 0x84007C, 0xB5004B, 0xE5001B,
        0xE81700, 0xAB7700, CRGB::Yellow, CRGB::Green,
        CRGB::LimeGreen, CRGB::DarkTurquoise, CRGB::CornflowerBlue, CRGB::Blue,
        CRGB::BlueViolet, CRGB::DeepPink, CRGB::Bisque, CRGB::White
 };
 extern const TProgmemPalette16 PartierColorsCompl_p FL_PROGMEM = {
         0x56AB00, 0x008408, 0x00A6B5, 0x008EE5,
         0x00D1E8, 0x0034AB, 0x8000FF, 0x800080,
         0xCD3280, 0xD10300, 0xED7864, 0xFFFF00,
         0x83E22B, 0x1493FF, 0xC6C4FF, 0xDC143C
 };

/**
 * Figures out the holiday type from a given time - based on month and day elements
 * @param time
 * @return one of the pre-defined holidays, defaults to Party
 */
Holiday colTheme::getHoliday(const time_t time) {
    const uint16_t md = encodeMonthDay(time);
    //Halloween: Oct 1 through Nov 3
    if (md > 0xA00 && md < 0xB04)
        return Halloween;
    //Thanksgiving: Nov 04 through Nov 30
    if (md > 0xB03 && md < 0xB1F)
        return Thanksgiving;
    //Christmas: Dec 23 through Dec 27
    if (md > 0xC16 && md < 0xC1C)
        return Christmas;
    //NewYear: Dec 30 through Jan 2
    if (md > 0xC1E || md < 0x103)
        return NewYear;
    //Party: all others (winter holidays)
    return Party;
}

Holiday colTheme::currentHoliday() {
    return timeStatus() != timeNotSet ? getHoliday(now()) : Party;
}

/**
 * Parse a string into a Holiday value - coded the hard way, supposedly a lookup map would be more convenient, but likely take more memory...
 * @param str
 * @return
 */
Holiday colTheme::parseHoliday(const String *str) {
    if (str->equalsIgnoreCase("Halloween"))
        return Halloween;
    else if (str->equalsIgnoreCase("Thanksgiving"))
        return Thanksgiving;
    else if (str->equalsIgnoreCase("Christmas"))
        return Christmas;
    else if (str->equalsIgnoreCase("NewYear"))
        return NewYear;
    return Party;
}

const char *colTheme::holidayToString(const Holiday hday) {
    switch (hday) {
        case None:
            return "None";
        case Party:
            return "Party";
        case Halloween:
            return "Halloween";
        case Thanksgiving:
            return "Thanksgiving";
        case Christmas:
            return "Christmas";
        case NewYear:
            return "NewYear";
        default:
            return "N/R";
    }
}

CRGBPalette16 PaletteFactory::mainPalette(const time_t time) {
    adjustHoliday(time);
    switch (holiday) {
        case Halloween:
            return HalloweenColors_p;
        case Thanksgiving:
            return HeatColors_p;
        case Christmas:
            return ChristmasColors_p;
        case NewYear:
            return Rainbow_gp;
        case Party:
        default:
            return PartierColors_p;
    }
}

CRGBPalette16 PaletteFactory::secondaryPalette(const time_t time) {
    adjustHoliday(time);
    switch (holiday) {
        case Halloween:
            return HalloweenStripeColors_p;
        case Thanksgiving:
            return LavaColors_p;
        case Christmas:
            return RainbowColors_p;
        case NewYear:
            return OceanColors_p;
        case Party:
        default:
            return PartierColorsCompl_p;
    }
}

void PaletteFactory::forceHoliday(const Holiday hday) {
    overrideHoliday = hday != None;
    holiday = hday;
}

Holiday PaletteFactory::adjustHoliday(const time_t time) {
    holiday = overrideHoliday ? holiday : (time == 0 ? ::currentHoliday() : getHoliday(time));
    return holiday;
}

Holiday PaletteFactory::currentHoliday() const {
    return holiday;
}

bool PaletteFactory::isHolidayLimitedHue() const {
    return currentHoliday() == Halloween;
}

CRGBPalette16 PaletteFactory::randomPalette(uint8_t ofsHue, time_t time) {
    if (time > 0)
        random16_add_entropy(time >> 4);
    return {CHSV(ofsHue + random8(0, 256-ofsHue), 160, random8(128,255)),
            CHSV(ofsHue + random8(0, 256-ofsHue), 255, random8(128,255)),
            CHSV(ofsHue + random8(), 192, random8(128,255)),
            CHSV(ofsHue + random8(), 255, random8(128,255))};
}

PaletteFactory paletteFactory;