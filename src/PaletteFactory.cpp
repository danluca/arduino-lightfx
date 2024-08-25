//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved.
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

extern const TProgmemPalette16 PatrioticColors_p FL_PROGMEM = {
        CRGB::DarkRed, CRGB::DarkRed, CRGB::Red, CRGB::Red,
        CRGB::Gray, CRGB::Gray, CRGB::White, CRGB::White,
        CRGB::DarkBlue, CRGB::DarkBlue, CRGB::Blue, CRGB::Blue,
        CRGB::White, CRGB::Red, CRGB::Blue, CRGB::Red
};

extern const TProgmemPalette16 ValentineColors_p FL_PROGMEM = {
        CRGB::DarkRed, CRGB::DarkRed, CRGB::Red, CRGB::Red,
        CRGB::DarkViolet, CRGB::DarkViolet, CRGB::Purple, CRGB::Purple,
        CRGB::Gray, CRGB::Gray, CRGB::White, CRGB::White,
        CRGB::Pink, CRGB::DeepPink, CRGB::HotPink, CRGB::LightPink
};

extern const TProgmemPalette16 PatrickColors_p FL_PROGMEM = {
        CRGB::DarkGreen, CRGB::DarkGreen, CRGB::Green, CRGB::Green,
        CRGB::GreenYellow, CRGB::GreenYellow, CRGB::YellowGreen, CRGB::YellowGreen,
        CRGB::DarkOliveGreen, CRGB::DarkSeaGreen, CRGB::ForestGreen, CRGB::LawnGreen,
        CRGB::LightGreen, CRGB::LightSeaGreen, CRGB::LimeGreen, CRGB::SpringGreen
};

extern const TProgmemPalette16 SleepyColors_p FL_PROGMEM = {
        0xc2893e, 0xc49d02, 0xcfbd1b, 0xd6d66b,
        0x54c414, 0x24d64e, 0x86b090, 0xd8dbc5,
        0x4bd195, 0x1cd6ba, 0xabdbd4, 0x49ebeb,
        0x49d0eb, 0xb242eb, 0xdd42eb, 0xeb42c0
};

// A mostly red palette with green accents and white trim.
// "CRGB::Gray" is used as white to keep the brightness more uniform.
extern const TProgmemPalette16 RedGreenWhite_p FL_PROGMEM = {
        CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red,
        CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red,
        CRGB::Red, CRGB::Red, CRGB::Gray, CRGB::Gray,
        CRGB::Green, CRGB::Green, CRGB::Green, CRGB::Green
};

// A pure "fairy light" palette with some brightness variations
#define HALFFAIRY ((CRGB::FairyLight & 0xFEFEFE) / 2)
#define QUARTERFAIRY ((CRGB::FairyLight & 0xFCFCFC) / 4)
extern const TProgmemPalette16 FairyLight_p FL_PROGMEM = {
        CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight,
        HALFFAIRY, HALFFAIRY, CRGB::FairyLight, CRGB::FairyLight,
        QUARTERFAIRY, QUARTERFAIRY, CRGB::FairyLight, CRGB::FairyLight,
        CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight
};

// A palette of soft snowflakes with the occasional bright one
extern const TProgmemPalette16 Snow_p FL_PROGMEM = {
        0x304048, 0x304048, 0x304048, 0x304048,
        0x304048, 0x304048, 0x304048, 0x304048,
        0x304048, 0x304048, 0x304048, 0x304048,
        0x304048, 0x304048, 0x304048, 0xE0F0FF
};

// A palette reminiscent of large 'old-school' C9-size tree lights
// in the five classic colors: red, orange, green, blue, and white.
#define C9_Red    0xB80400
#define C9_Orange 0x902C02
#define C9_Green  0x046002
#define C9_Blue   0x070758
#define C9_White  0x606820
extern const TProgmemPalette16 RetroC9_p FL_PROGMEM = {
        C9_Red, C9_Orange, C9_Red, C9_Orange,
        C9_Orange, C9_Red, C9_Orange, C9_Red,
        C9_Green, C9_Green, C9_Green, C9_Green,
        C9_Blue, C9_Blue, C9_Blue, C9_White
};

// A cold, icy pale blue palette
#define Ice_Blue1 0x0C1040
#define Ice_Blue2 0x182080
#define Ice_Blue3 0x5080C0
extern const TProgmemRGBPalette16 Ice_p FL_PROGMEM = {
        Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
        Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
        Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
        Ice_Blue2, Ice_Blue2, Ice_Blue2, Ice_Blue3
};


static const TProgmemPalette16 *allPalettes[] = {
        &HalloweenColors_p, &HalloweenStripeColors_p, &ChristmasColors_p, &PartierColors_p,
        &PartierColorsCompl_p, &PatrioticColors_p, &ValentineColors_p, &PatrickColors_p,
        &SleepyColors_p, &RedGreenWhite_p, &FairyLight_p, &Snow_p, &RetroC9_p,
        &Ice_p
};

uint8_t PaletteFactory::activePaletteIndex = 0;

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
        case MemorialDay:
        case IndependenceDay:
            return PatrioticColors_p;
        case ValentineDay:
            return ValentineColors_p;
        case StPatrick:
            return PatrickColors_p;
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
        case MemorialDay:
        case IndependenceDay:
            return PatrioticColors_p;
        case ValentineDay:
            return LavaColors_p;
        case StPatrick:
            return ForestColors_p;
        case Party:
        default:
            return PartierColorsCompl_p;
    }
}

void PaletteFactory::toHSVPalette(CHSVPalette16 &hsvPalette, const CRGBPalette16 &rgbPalette) {
    for (int i = 0; i < 16; i++) {
        hsvPalette[i] = rgb2hsv_approximate(rgbPalette[i]);
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
    return {CHSV(ofsHue + random8(0, 256 - ofsHue), 160, random8(128, 255)),
            CHSV(ofsHue + random8(0, 256 - ofsHue), 255, random8(128, 255)),
            CHSV(ofsHue + random8(), 192, random8(128, 255)),
            CHSV(ofsHue + random8(), 255, random8(128, 255))};
}

CRGBPalette16 PaletteFactory::selectRandomPalette() {
    return *allPalettes[random8(0, sizeof(allPalettes)/sizeof(TProgmemPalette16*))];
}

CRGBPalette16 PaletteFactory::selectNextPalette() {
    activePaletteIndex = addmod8(activePaletteIndex, 1, sizeof(allPalettes)/sizeof(TProgmemPalette16*));
    return *allPalettes[activePaletteIndex];
}

PaletteFactory paletteFactory;