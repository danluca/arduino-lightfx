//
// Copyright (c) 2023 by Dan Luca. All rights reserved.
//

#include "PaletteFactory.h"

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

bool PaletteFactory::isAuto() const {
    return autoChangeHoliday;
}

void PaletteFactory::setAuto(bool autoMode) {
    autoChangeHoliday = autoMode;
}

void PaletteFactory::setHoliday(const Holiday hday) {
    bool noPref = hday == None;
    setAuto(noPref);
    if (noPref)
        adjustHoliday();
    else
        holiday = hday;
}

Holiday PaletteFactory::adjustHoliday(const time_t time) {
    //if no auto-adjust or no WiFi then return the current holiday. At bootstrap, until WiFi connects, the state restore will seed the previously saved holiday
    if (!autoChangeHoliday || !isSysStatus(SYS_STATUS_WIFI))
        return holiday;
    holiday = time == 0 ? currentHoliday() : buildHoliday(time);
    return holiday;
}

Holiday PaletteFactory::getHoliday() const {
    return holiday;
}

bool PaletteFactory::isHolidayLimitedHue() const {
    return getHoliday() == Halloween;
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