// Copyright (c) 2023 by Dan Luca. All rights reserved.
//

#include "util.h"
// increase the amount of space for file system to 128kB (default 64kB)
#define RP2040_FS_SIZE_KB   (128)
#include <LittleFSWrapper.h>

#define FILE_BUF_SIZE   256
const uint maxAdc = 1 << ADC_RESOLUTION;
const char stateFileName[] = LITTLEFS_FILE_PREFIX "/state.json";

LittleFSWrapper *fsPtr;

int boardTemperature() {
    bool result = IMU.temperatureAvailable();
    if (result) {
        int temperature_deg = 0;
        IMU.readTemperature(temperature_deg);
#ifndef DISABLE_LOGGING
        // Serial console doesn't seem to work well with UTF-8 chars, hence not using ° symbol for degree.
        // Can also try using wchar_t type. Unsure ArduinoLog library supports it well. All in all, not worth digging much into it - only used for troubleshooting
        Log.infoln(F("Board temperature %d 'C (%F 'F)"), temperature_deg, temperature_deg*9.0/5+32);
#endif
        return temperature_deg;
    }
    return IMU_TEMPERATURE_NOT_AVAILABLE;
}

float controllerVoltage() {
    const uint avgSize = 8;  //we'll average 8 readings back to back
    uint valSum = 0;
    for (uint x = 0; x < avgSize; x++)
        valSum += analogRead(A0);
    Log.infoln(F("Voltage %d average reading: %d"), avgSize, valSum/avgSize);
    valSum = valSum*MV3_3/avgSize;
    valSum = valSum*(VCC_DIV_R5+VCC_DIV_R4)/VCC_DIV_R5/maxAdc;
    return (float)valSum/1000.0f;
}

float chipTemperature() {
    uint curAdc = adc_get_selected_input();
    const uint avgSize = 8;   //we'll average 8 readings back to back

    adc_select_input(4);
    uint valSum = 0;
    for (uint x = 0; x < avgSize; x++)
        valSum += adc_read();
    adc_select_input(curAdc);   //restore the ADC input selection
    Log.infoln(F("Internal Temperature %d average reading: %d"), avgSize, valSum/avgSize);

    uint tV = valSum*MV3_3/avgSize/maxAdc;   //voltage in mV
    return 27.0f - (float)(tV - 706)/1.721f;
}

void fsInit() {
#ifndef DISABLE_LOGGING
    mbed::BlockDevice *bd = mbed::BlockDevice::get_default_instance();
    bd->init();
    Log.infoln(F("Default BlockDevice type %s, size %u B, read size %u B, program size %u B, erase size %u B"),
               bd->get_type(), bd->size(), bd->get_read_size(), bd->get_program_size(), bd->get_erase_size());
    bd->deinit();
#endif

    fsPtr = new LittleFSWrapper();
    if (fsPtr->init())
        Log.infoln("Filesystem OK");

#ifndef DISABLE_LOGGING
    Log.infoln(F("Root FS %s contents:"), LittleFSWrapper::getRoot());
    DIR *d = opendir(LittleFSWrapper::getRoot());
    struct dirent *e = nullptr;
    while (e = readdir(d))
        Log.infoln(F("  %s [%d]"), e->d_name, e->d_type);
    Log.infoln(F("Dir complete."));
#endif
}


/**
 * Reads a text file - if it exists - into a string object
 * @param fname name of the file to read
 * @param s string to store contents into
 * @return number of characters in the file; 0 if file is empty or doesn't exist
 */
size_t readTextFile(const char *fname, String *s) {
    FILE *f = fopen(fname, "r");
    size_t fsize = 0;
    if (f) {
        char buf[FILE_BUF_SIZE];
        memset(buf, 0, FILE_BUF_SIZE);
        size_t cread = 1;
        while (cread = fread(buf, 1, FILE_BUF_SIZE, f)) {
            s->concat(buf, cread);
            fsize += cread;
        }
        fclose(f);
#ifndef DISABLE_LOGGING
        Log.infoln(F("Read %d bytes from %s file"), fsize, fname);
        Log.traceln(F("File %s content [%d]: %s"), fname, fsize, s->c_str());
#endif
    } else
        Log.errorln(F("Text file %s was not found/could not read"), fname);
    return fsize;
}

/**
 * Writes (overrides if already exists) a file using the string content
 * @param fname file name to write
 * @param s contents to write
 * @return number of bytes written
 */
size_t writeTextFile(const char *fname, String *s) {
    size_t fsize = 0;
    FILE *f = fopen(fname, "w");
    if (f) {
        fsize = fwrite(s->c_str(), sizeof(s->charAt(0)), s->length(), f);
        fclose(f);
        Log.infoln(F("File %s has been saved, size %d bytes"), fname, s->length());
    } else
        Log.errorln(F("Failed to create file %s for writing"), fname);
    return fsize;
}

bool removeFile(const char *fname) {
    FILE *f = fopen(fname, "r");
    if (f) {
        fclose(f);
        return lfs.remove(fname) == 0;
    }
    //file does not exist - return true to the caller, the intent is already fulfilled
    return true;
}

/**
 * Ease Out Bounce implementation - leverages the double precision original implementation converted to int in a range
 * @param x input value
 * @param lim high limit range
 * @return the result in [0,lim] inclusive range
 * @see https://easings.net/#easeOutBounce
 */
uint16_t easeOutBounce(const uint16_t x, const uint16_t lim) {
    static const float d1 = 2.75f;
    static const float n1 = 7.5625f;

    float xf = ((float)x)/(float)lim;
    float res = 0;
    if (xf < 1/d1) {
        res = n1*xf*xf;
    } else if (xf < 2/d1) {
        float xf1 = xf - 1.5f/d1;
        res = n1*xf1*xf1 + 0.75f;
    } else if (xf < 2.5f/d1) {
        float xf1 = xf - 2.25f/d1;
        res = n1*xf1*xf1 + 0.9375f;
    } else {
        float xf1 = xf - 2.625f/d1;
        res = n1*xf1*xf1 + 0.984375f;
    }
    return (uint16_t )(res * (float)lim);
}

/**
 * Multiply function for color blending purposes - assumes the operands are fractional (n/256) and the result
 * of multiplying 2 fractional numbers is less than both numbers (e.g. 0.2 x 0.3 = 0.06). Special handling for operand values of 255 (i.e. 1.0)
 * <p>f(a,b) = a*b</p>
 * @param a first operand, range [0,255] inclusive - mapped as range [0,1.0] inclusive
 * @param b second operand, range [0,255] inclusive - mapped as range [0,1.0] inclusive
 * @return (a*b)/256
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
uint8_t bmul8(uint8_t a, uint8_t b) {
    if (a==255)
        return b;
    if (b==255)
        return a;
    return ((uint16_t)a*(uint16_t)b)/256;
}

/**
 * Screen two 8 bit operands for color blending purposes - assumes the operands are fractional (n/256)
 * <p>f(a,b)=1-(1-a)*(1-b)</p>
 * @param a first operand, range [0,255] inclusive - mapped as range [0,1.0] inclusive
 * @param b second operand, range [0,255] inclusive - mapped as range [0,1.0] inclusive
 * @return 255-bmul8(255-a, 255-b)
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
uint8_t bscr8(uint8_t a, uint8_t b) {
    return 255-bmul8(255-a, 255-b);
}

/**
 * Overlay two 8 bit operands for color blending purposes - assumes the operands are fractional (n/256)
 * <p>f(a,b)=2*a*b, if a<0.5; 1-2*(1-a)*(1-b), otherwise</p>
 * @param a first operand, range [0,255] inclusive - mapped as range [0,1.0] inclusive
 * @param b second operand, range [0,255] inclusive - mapped as range [0,1.0] inclusive
 * @return the 8 bit value per formula above
 * @see https://en.wikipedia.org/wiki/Blend_modes
 */
uint8_t bovl8(uint8_t a, uint8_t b) {
    if (a < 128)
        return bmul8(a, b)*2;
    return 255-bmul8(255-a, 255-b)*2;
}
