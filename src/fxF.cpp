//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//
/**
 * Category F of light effects
 *
 */
#include "fxF.h"
#include <vector>
#include <algorithm>

using namespace FxF;
using namespace colTheme;

//~ Effect description strings stored in flash
const char fxf1Desc[] PROGMEM = "FxF1: beat wave";
const char fxf2Desc[] PROGMEM = "FxF2: Halloween breathe with various color blends";
const char fxf3Desc[] PROGMEM = "FxF3: Eye Blink";
const char fxf4Desc[] PROGMEM = "FxF4: Bouncy segments";
const char fxf5Desc[] PROGMEM = "FxF5: Fireworks";

void FxF::fxRegister() {
    static FxF1 fxF1;
    static FxF2 fxF2;
    static FxF3 fxF3;
    static FxF4 fxF4;
    static FxF5 fxF5;
}

// FxF1
FxF1::FxF1() : LedEffect(fxf1Desc) {}

void FxF1::setup() {
    LedEffect::setup();
    speed = 60;
    fade = 96;
    hue = random8();
    hueDiff = 8;
}

void FxF1::run() {
    EVERY_N_MILLISECONDS(speed) {
        const uint8_t dotSize = 2;
        tpl.fadeToBlackBy(fade);

        uint16_t w1 = (beatsin16(12, 0, tpl.size()-dotSize-1) + beatsin16(24, 0, tpl.size()-dotSize-1))/2;
        uint16_t w2 = beatsin16(14, 0, tpl.size()-dotSize-1, 0, beat8(10)*128);

        CRGB clr1 = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
        CRGB clr2 = ColorFromPalette(targetPalette, hue, brightness, LINEARBLEND);

        CRGBSet seg1 = tpl(w1, w1+dotSize);
        seg1 = clr1;
        seg1.blur1d(64);
        CRGBSet seg2 = tpl(w2, w2+dotSize);
        seg2 |= clr2;

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
        hue += hueDiff;
    }
}

bool FxF1::windDown() {
    return transEffect.offSpots();
}

uint8_t FxF1::selectionWeight() const {
    return 12;
}

// FxF2
FxF2::FxF2() : LedEffect(fxf2Desc), pattern(frame(0, FRAME_SIZE-1)) {
}

void FxF2::setup() {
    LedEffect::setup();
    hue = 2;
    hueDiff = 7;
    makePattern(hue);
}

void FxF2::run() {
    // frame rate - 20fps
    EVERY_N_MILLISECONDS(50) {
        double dBreath = (exp(sin(millis()/2400.0*PI)) - 0.36787944)*108.0;//(exp(sin(millis()/2000.0*PI)) - 0.36787944)*108.0;       //(exp(sin(millis()/4000.0*PI)) - 0.36787944)*108.0;//(exp(sin(millis()/2000.0*PI)) - 0.36787944)*108.0;
        uint8_t breathLum = map(dBreath, 0, 255, 0, BRIGHTNESS);
        CRGB clr = ColorFromPalette(palette, hue, breathLum, LINEARBLEND);
        for (CRGBSet::iterator m=pattern.begin(), p=tpl.begin(), me=pattern.end(), pe=tpl.end(); m!=me && p!=pe; ++m, ++p) {
            if ((*m) == CRGB::White)
                (*p) = clr;
        }
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);

        if (breathLum < 2) {
            hue += hueDiff;
            makePattern(hue);
        }
    }
}

void FxF2::makePattern(uint8_t hue) {
    //clear the pattern, start over
    pattern = CRGB::Black;
    uint16_t s0 = random8(17);
    // pattern of XXX--XX-X-XX--XXX
    for (uint16_t i = 0; i < pattern.size(); i+=17) {
        pattern(i, i+2) = CRGB::White;
        pattern(i+5, i+6) = CRGB::White;
        pattern[i+8] = CRGB::White;
        pattern(i+10, i+11) = CRGB::White;
        pattern(i+14, i+16) = CRGB::White;
    }
    loopRight(pattern, (Viewport)0, s0);
}

bool FxF2::windDown() {
    return transEffect.offWipe(false);
}

uint8_t FxF2::selectionWeight() const {
    return paletteFactory.getHoliday() == Halloween ? 42 : 24;
}

// FxF3
FxF3::FxF3() : LedEffect(fxf3Desc) {}

void FxF3::setup() {
    LedEffect::setup();
    hue = random8();
    hueDiff = 11;
    //reset all eyes
    for (auto & e : eyes)
        e.reset(0, BKG);
}

void FxF3::run() {
    EVERY_N_SECONDS(5) {
        //activate eyes if possible
        uint8_t numEyes = 1 + random8(maxEyes);
        for (uint8_t i = 0; i < numEyes; i++) {
            Viewport v = nextEyePos();
            if (v.size() > 0) {
                EyeBlink *availEye = findAvailableEye();
                if (availEye) {
                    availEye->reset(random16(v.low, v.high),
                        ColorFromPalette(palette, hue, brightness, LINEARBLEND));
                    availEye->start();
                    hue += hueDiff;
                }
            }
        }
    }
    EVERY_N_MILLISECONDS(60) {
        //step advance each active eye
        for (auto & eye : eyes)
            eye.step();
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

/**
 * Find the first available eye - inactive, that is
 * @return pointer to first inactive eye, nullptr if all eyes are active
 */
EyeBlink * FxF3::findAvailableEye() {
    for (auto & eye : eyes) {
        if (!eye)
            return &eye;
    }
    return nullptr;
}

/**
 * Find the largest gap in between the currently active eyes and return a range of possible new eye positions
 * <p>It accounts for eye's size, hence the Viewport returned does accurately reflect all possible start eye positions (and the whole eye would still fit)</p>
 * @return the viewport of the largest gap; a viewport of size 0 if no gaps exists for an eye to fit in
 */
Viewport FxF3::nextEyePos() {
    //find active eyes
    std::vector<EyeBlink*> actEyes;
    for (auto & e : eyes) {
        if (e)
            actEyes.push_back(&e);
    }
    //if no active eyes (like beginning) return the full tpl strip
    if (actEyes.empty())
        return {0, static_cast<uint16_t>(tpl.size() - EyeBlink::size)};

    //sort active eyes ascending by position - notice the use of lambda expression for custom comparator (available since C++11, we're using C++14) - cool stuff!!
    std::sort(actEyes.begin(), actEyes.end(), [](EyeBlink *a, EyeBlink *b) {return a->pos < b->pos;});
    //find the gaps
    uint16_t posGap = 0, szGap = 0, prevEyeEnd = 0, curGap = 0;
    for (auto & actEye : actEyes) {
        curGap = actEye->pos - prevEyeEnd;
        if (curGap > szGap) {
            posGap = prevEyeEnd;
            szGap = curGap;
        }
        prevEyeEnd = actEye->pos + EyeBlink::size;
    }
    //gap between last active eye and upper end of the template strip
    curGap = tpl.size() - prevEyeEnd;
    if (curGap > szGap) {
        posGap = prevEyeEnd;
        szGap = curGap;
    }
    return szGap > EyeBlink::size ? Viewport(posGap, posGap+szGap-EyeBlink::size) : (Viewport)0;
}

bool FxF3::windDown() {
    return transEffect.offWipe(true);
}

uint8_t FxF3::selectionWeight() const {
    return paletteFactory.getHoliday() == Halloween ? 42 : 24;
}

/**
 * Default constructor - initializes all fields with sensible values
 */
EyeBlink::EyeBlink() : curStep(Off), holderSet(&tpl), color(CRGB::Red) {
    //initialize the inner fields with default values
    idleTime = 100;  //100 cycles before this eye can be used again (6s at 60ms time base)
    brIncr = 80;    //each step brightness is increased/decreased by this amount
    numBlinks = 2;  //blink twice before deactivating
    curBrightness = 0; //initial brightness starting from black
    curLen = 0;     //eyes closed initially
    pos = 0;        //default position
    curPause = pauseTime = 20; //pause 1.2s (with 60ms time base in loop - see every_n_milliseconds speed) between opening/closing lids
}

/**
 * Advances the state machine of the eye blinker
 */
void EyeBlink::step() {
    if (!isActive())
        return;
    CRGBSet eye = (*holderSet)(pos, pos+size-1);
    uint8_t halfEyeSize = eyeSize/2;
    switch (curStep) {
        case OpenLid:
            curBrightness = qadd8(curBrightness, brIncr);
            eye[eyeSize+halfEyeSize+eyeGapSize+curLen] = eye[eyeSize+halfEyeSize+eyeGapSize-curLen] = eye[halfEyeSize+curLen] = eye[halfEyeSize-curLen] = adjustBrightness(color, curBrightness);
            if (curBrightness == 255) {
                curBrightness = 0;
                curLen++;
            }
            if (curLen > halfEyeSize) {
                curPause = pauseTime;
                curStep = PauseOn;
            }
            break;
        case PauseOn:
            curPause--;
            if (curPause == 0) {
                curBrightness = 255;
                curLen = 0;
                curStep = CloseLid;
            }
            break;
        case CloseLid:
            curBrightness = qsub8(curBrightness, brIncr);
            eye[eyeSize*2+eyeGapSize-curLen-1] = eye[eyeSize+eyeGapSize+curLen] = eye[eyeSize-curLen-1] = eye[curLen] = adjustBrightness(color, curBrightness);
            if (curBrightness == 0) {
                curBrightness = 255;
                curLen++;
            }
            if (curLen > halfEyeSize) {
                curPause = pauseTime/3; //staying with lids closed is much shorter than with them open
                curStep = PauseOff;
            }
            break;
        case PauseOff:
            curPause--;
            if (curPause == 0) {
                curBrightness = 0;
                curLen = 0;
                curStep = --numBlinks ? OpenLid : Idle;
            }
            break;
        case Idle:
            idleTime--;
            if (idleTime == 0)
                curStep = Off;
            break;
    }
}

/**
 * Makes the eye active and reset its state machine
 */
void EyeBlink::start() {
    curStep = OpenLid;
    curLen = 0;
    curBrightness = 0;
}

/**
 * Changes position and color of the eyes, resets all the state machine parameters
 * @param curPos new position to use
 * @param clr new color to use
 */
void EyeBlink::reset(uint16_t curPos, CRGB clr) {
    curStep = Off;
    idleTime = random16(80, 160);    //5-10s of idle time
    brIncr = random8(50, 100);
    numBlinks = random8(2, 6);
    curPause = pauseTime = random8(30, 60);    //1.8-3.6s between opening/closing lids (with 60ms time base in looping)
    pos = curPos;
    color = clr;
    curLen = 0;
}

/**
 * Is the eye currently active?
 * @return true if state machine is not in the Off state; false otherwise
 */
bool EyeBlink::isActive() const {
    return curStep != Off;
}

// FxF4
void FxF4::setup() {
    LedEffect::setup();
    hue = 0;
    hueDiff = 11;
    curPos = 0;
    delta = 0;
    dirFwd = true;
    brightness = 200;
    dist = 0;
    ofs = random8(wiggleRoom);
}

void FxF4::run() {
    EVERY_N_MILLISECONDS_I(fxf4Timer, 50) {
        switch (fxState) {
            case Bounce:
                if (delta > 0) {
                    CRGB feed = curPos > dotSize ? BKG : ColorFromPalette(palette, hue+=hueDiff, brightness, LINEARBLEND);
                    if (dirFwd) {
                        shiftRight(set1, feed);
                        curPos++;
                    } else {
                        shiftLeft(set1, feed);
                        curPos--;
                    }
                    set2 = set1;
                    tpl = frame(ofs, ofs+tpl.size()-1);
                    delta--;
                } else {
                    uint16_t easePos = bouncyCurve[dist++];
                    if (dist > upLim) {
                        fxState = Reduce;
                        delta = dotSize-1;
                    } else if (easePos > 0) {
                        //skip the 0 values of the bouncy curve
                        delta = asub(easePos, curPos);  //for the current frame size, delta doesn't go above 5. For larger sizes,  the max is 6.
                        dirFwd = easePos > curPos;
                        fxf4Timer.setPeriod(10 + (50 - delta * 8)); //speeds between 60ms - 20ms
                    }
                }
                break;
            case Reduce:
                if (delta > 0) {
                    shiftRight(set1, BKG);
                    set2 = set1;
                    tpl = frame(ofs, ofs+tpl.size()-1);
                    delta--;
                } else {
                    fxState = Flash;
                    delta = 1;  //1 time cycle for flash
                }
                break;
            case Flash:
                if (delta > 0) {
                    set1[set1.size()-1] = set2[set2.size() - 1] = CRGB::White;
                    tpl = frame(ofs, ofs+tpl.size()-1);
                    delta--;
                    fxf4Timer.setPeriod(10);
                } else {
                    //turn off flash pixels
                    set1[set1.size()-1] = set2[set2.size() - 1] = BKG;
                    tpl = frame(ofs, ofs+tpl.size()-1);
                    //start over
                    curPos = 0;
                    delta = 0;
                    dist = 0;
                    ofs = random8(wiggleRoom);
                    fxState = Bounce;
                    fxf4Timer.setPeriod(50);
                }
                break;
        }

        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

void FxF4::offsetBounce(const CRGB &feed) const {
    if (ofs != 0) {
        if (curPos >= dotSize) {
            if (ofs > 0)
                shiftRight(tpl, BKG, (Viewport) 0, ofs);
            else
                shiftLeft(tpl, BKG, (Viewport) 0, abs(ofs));
        } else {
            if (abs(ofs) + curPos > dotSize) {
                if (ofs > 0) {
                    shiftRight(tpl, feed, (Viewport) 0, dotSize - curPos);
                    shiftRight(tpl, BKG, (Viewport) 0, ofs + curPos - dotSize);
                } else {
                    shiftLeft(tpl, feed, (Viewport) 0, dotSize - curPos);
                    shiftLeft(tpl, BKG, (Viewport) 0, abs(ofs) + curPos - dotSize);
                }
            } else {
                if (ofs > 0)
                    shiftRight(tpl, feed, (Viewport) 0, ofs);
                else
                    shiftLeft(tpl, feed, (Viewport) 0, abs(ofs));
            }
        }
    }
}

FxF4::FxF4() : LedEffect(fxf4Desc), fxState(Bounce), set1(frame(0, (tpl.size() + wiggleRoom)/ 2 - 1)), set2(frame(tpl.size() + wiggleRoom - 1, (tpl.size() + wiggleRoom)/2)) {
    ofs = wiggleRoom/2;
    for (short x = 0; x < upLim; x++)
        bouncyCurve[x] = easeOutBounce(x, upLim - 1);

}

bool FxF4::windDown() {
    return transEffect.offWipe(true);
}

uint8_t FxF4::selectionWeight() const {
    return paletteFactory.getHoliday() == Halloween ? 12 : 42;
}

// FxF5 - algorithm by Carl Rosendahl, adapted from code published at https://www.anirama.com/1000leds/1d-fireworks/
// HEAVY floating point math
FxF5::FxF5() : LedEffect(fxf5Desc) {}

void FxF5::run() {
    EVERY_N_MILLIS_I(fxf5Timer, 1000) {
        flare();

        explode();

        fxf5Timer.setPeriod(random16(1000, 4000));
    }
}

void FxF5::setup() {
    LedEffect::setup();
}

/**
 * Send up a flare
 */
void FxF5::flare() {
    const ushort flareSparksCount = 3;
    float flareStep = flarePos = 0;
    bFade = random8() % 2;
    curPos = random16(tpl.size()*explRangeLow/10, tpl.size()*explRangeHigh/10);
    float flareVel = float(random16(400, 650)) / 1000; // trial and error to get reasonable range to match the 30-80 % range of the strip height we want
    float flBrightness = 255;
    Spark flareSparks[flareSparksCount];

    // initialize launch sparks
    for (auto &spark : flareSparks) {
        spark.pos = 0;
        spark.velocity = (float(random8(180,255)) / 255) * (flareVel / 2);
        // random around 20% of flare velocity
        spark.hue = uint8_t(spark.velocity * 1000);
    }
    // launch
    while ((ushort(flarePos) < curPos) && (flareVel > 0)) {
        tpl = BKG;
        // sparks
        for (auto &spark : flareSparks) {
            spark.pos += spark.velocity;
            spark.limitPos(curPos);
            spark.velocity += gravity;
            spark.hue = capd(qsuba(spark.hue, 1), 64);
            tpl[spark.iPos()] = HeatColor(spark.hue);
            tpl[spark.iPos()] %= 50; // reduce brightness to 50/255
        }

        // flare
        flarePos = easeOutQuad(ushort(flareStep), curPos);
        tpl[ushort(flarePos)] = CHSV(0, 0, ushort(flBrightness));
        replicateSet(tpl, others);
        flareStep += flareVel;
        //flarePos = constrain(flarePos, 0, curPos);
        flareVel += gravity;
        flBrightness *= .985;

        FastLED.show(stripBrightness);
    }
}

/**
 * Explode!
 *
 * Explosion happens where the flare ended.
 * Size is proportional to the height.
 */
void FxF5::explode() const {
    const auto nSparks = ushort(flarePos / 3); // works out to look about right
    Spark sparks[nSparks];
    //map the flare position in its range to a hue
    uint8_t decayHue = constrain(map(ushort(flarePos), tpl.size()*explRangeLow/10, tpl.size()*explRangeHigh/10, 0, 255), 0, 255);
    uint8_t flarePosQdrnt = decayHue/64;

    // initialize sparks
    for (auto &spark : sparks) {
        spark.pos = flarePos;
        spark.velocity = (float(random16(0, 20000)) / 10000.0f) - 1.0f; // from -1 to 1
        spark.hue = random8(flarePosQdrnt*64, 64+flarePosQdrnt*64);   //limit the spark hues in a closer color range based on flare height
        spark.velocity *= flarePos/1.7f/ float(tpl.size()); // proportional to height
    }
    // the original implementation was to designate a known spark starting from a known value and iterate until it goes below a fixed threshold - c2/128
    // since they were all fixed values, the math shows the number of iterations can be precisely determined. The formula is iterCount = log(c2/128/255)/log(degFactor),
    // rounded up to nearest integer. For instance, for original values of c2=50, degFactor=0.99, we're looking at 645 loops. With some experiments, I've landed
    // at c2=30, degFactor=0.987, looping at 535 loops.
    const ushort loopCount = 540;
    float dying_gravity = gravity;
    ushort iter = 0;
    bool activeSparks = true;
    while((iter++ < loopCount) && activeSparks) {
        if (bFade)
            tpl.fadeToBlackBy(9);
        else
            tpl = BKG;
        activeSparks = false;
        for (auto &spark : sparks) {
            if (spark.iPos() == 0)
                continue;   //if this spark has reached bottom, save our breath
            activeSparks = true;
            spark.pos += spark.velocity;
            spark.limitPos(float(tpl.size()-1));
            spark.velocity += dying_gravity;
            //spark.colorFade *= .987f;       //degradation factor degFactor in formula above
            //fade the sparks
//            auto spDist = uint8_t(abs(spark.pos - flarePos) * 255 / flarePos);
            auto spDist = uint8_t(abs(spark.pos - flarePos));
            ushort tplPos = spark.iPos();
            if (bFade) {
                tpl[tplPos] += ColorFromPalette(palette, spark.hue+spDist, 255-2*spDist);
                //tpl.blur1d();
            } else {
                tpl[tplPos] = blend(ColorFromPalette(palette, spark.hue),
                                    CHSV(decayHue, 224, 255-2*spDist),
                                    3*spDist);
            }
        }

        dying_gravity *= 0.985; // as sparks burn out they fall slower
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
    tpl = BKG;
    replicateSet(tpl, others);
    FastLED.show(stripBrightness);
}

bool FxF5::windDown() {
    return transEffect.offWipe(true);
}

uint8_t FxF5::selectionWeight() const {
    return paletteFactory.getHoliday() == Halloween ? 10 : 64;
}
