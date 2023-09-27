/**
 * Category F of light effects
 *
 */
#include "fxF.h"
#include <vector>
#include <algorithm>

using namespace FxF;

void FxF::fxRegister() {
    static FxF1 fxF1;
    static FxF2 fxF2;
    static FxF3 fxF3;
}

FxF1::FxF1() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxF1::setup() {
    resetGlobals();
    speed = 60;
    fade = 96;
    hue = random8();
    hueDiff = 8;
}

void FxF1::loop() {
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

const char *FxF1::description() const {
    return "FxF1: beat wave";
}

const char *FxF1::name() const {
    return "FxF1";
}

void FxF1::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

// FxF2
FxF2::FxF2() : pattern(frame(0, FRAME_SIZE-1)) {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxF2::setup() {
    resetGlobals();
    hue = 2;
    hueDiff = 7;
    makePattern(hue);
}

void FxF2::loop() {
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

const char *FxF2::description() const {
    return "FxF2: Halloween breathe with various color blends";
}

const char *FxF2::name() const {
    return "FxF2";
}

void FxF2::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

// FxF3
FxF3::FxF3() {
    registryIndex = fxRegistry.registerEffect(this);
}

void FxF3::setup() {
    resetGlobals();
    hue = 4;
    hueDiff = 11;
    //reset all eyes
    for (auto & e : eyes)
        e.reset(0, BKG);
}

void FxF3::loop() {
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
    EVERY_N_MILLISECONDS(50) {
        //step advance each active eye
        for (auto & eye : eyes)
            eye.step();
        replicateSet(tpl, others);
        FastLED.show(stripBrightness);
    }
}

const char *FxF3::description() const {
    return "FxF3: Eye Blink";
}

const char *FxF3::name() const {
    return "FxF3";
}

void FxF3::describeConfig(JsonArray &json) const {
    JsonObject obj = json.createNestedObject();
    baseConfig(obj);
}

/**
 * Find the index of first available eye - inactive, that is
 * @return index of first inactive eye, -1 if all eyes are active
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
        return {0, static_cast<uint16_t>(tpl.size())};

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
    //gap between last active eye and high end of template strip
    curGap = tpl.size() - prevEyeEnd;
    if (curGap > szGap) {
        posGap = prevEyeEnd;
        szGap = curGap;
    }
    return szGap > EyeBlink::size ? Viewport(posGap, posGap+szGap-EyeBlink::size) : (Viewport)0;
}

EyeBlink::EyeBlink() : curStep(Off), holderSet(&tpl), color(CRGB::Red) {
    //initialize the inner fields with default values
    idleTime = 10;  //10 cycles before this eye can be used again (0.5s at 50ms time base)
    brIncr = 80;    //each step brightness is increased/decreased by this amount
    numBlinks = 2;  //blink twice before deactivating
    curBrightness = 0; //initial brightness starting from black
    curLen = 0;     //eyes closed initially
    pos = 0;        //default position
    curPause = pauseTime = 20; //pause 1s (with 50ms time base in loop - see every_n_milliseconds speed) between opening/closing lids
}

void EyeBlink::step() {
    if (!isActive())
        return;
    CRGBSet eye = (*holderSet)(pos, pos+size-1);
    uint8_t halfEyeSize = eyeSize/2;
    uint8_t x = 0;
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

void EyeBlink::start() {
    curStep = OpenLid;
    curLen = 0;
    curBrightness = 0;
}

void EyeBlink::reset(uint16_t curPos, CRGB clr) {
    curStep = Off;
    idleTime = random16(30, 60);
    brIncr = random8(40, 120);
    numBlinks = random8(2, 6);
    curPause = pauseTime = random8(20, 40);    //1-2s between opening/closing lids (with 50ms time base in looping)
    pos = curPos;
    color = clr;
    curLen = 0;
}

bool EyeBlink::isActive() const {
    return curStep != Off;
}
