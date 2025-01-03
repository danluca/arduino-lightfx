//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved.
//
#include <ArduinoECCX08.h>
#include "utility/ECCX08DefaultTLSConfig.h"
#include <FreeRTOS.h>
#include "timeutil.h"
#include "hardware/watchdog.h"
#include "FastLED.h"
#include "sysinfo.h"
#include "util.h"
#include "stringutils.h"
#include "log.h"


FixedQueue<TimeSync, 8> timeSyncs;

/**
 * Adapted from article https://rheingoldheavy.com/better-arduino-random-values/
 * <p>Not fast, timed at 64ms on Arduino Uno allegedly</p>
 * <p>The residual floating voltage of the unconnected AD pin A1 is read multiple times and assembled into a 32 bit value</p>
 * @return a good 32bit random value from the residual electric signal on an Analog-Digital unconnected (floating) pin
 */
ulong adcRandom() {
    uint8_t  seedBitValue  = 0;
    uint8_t  seedByteValue = 0;
    uint32_t seedWordValue = 0;

    for (uint8_t wordShift = 0; wordShift < 4; wordShift++) {       // 4 bytes in a 32 bit word
        for (uint8_t byteShift = 0; byteShift < 8; byteShift+=2) {  // 8 bits in a byte, advance by 2 - we collect 2 bits (last 2 LSB) with each analog read
            for (uint8_t bitSum = 0; bitSum < 8; bitSum++) {        // 8 samples of analog pin
                seedBitValue += (analogRead(A1) & 0x03);  // Flip the coin 8 times, adding the results together
            }
            taskDelay(1);                                               // Delay a single millisecond to allow the pin to fluctuate
            seedByteValue |= ((seedBitValue & 0x03) << byteShift);  // Build a stack of eight flipped coins
            seedBitValue = 0;                                       // Clear out the previous coin value
        }
        seedWordValue |= ((uint32_t)seedByteValue << (8 * wordShift));    // Build a stack of four sets of 8 coins (shifting right creates a larger number so cast to 32bit)
        seedByteValue = 0;                                                // Clear out the previous stack value
    }
    return (seedWordValue);
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
 * <p>f(a,b)=2*a*b, if a&lt;0.5; 1-2*(1-a)*(1-b), otherwise</p>
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

/**
 * Leverages ECC608B's High-Quality NIST SP 800-90A/B/C Random Number Generator
 * <p>It is slow - takes about 30ms</p>
 * @param minLim minimum value, defaults to 0
 * @param maxLim maximum value, defaults to max unsigned 8 bits (UINT8_MAX)
 * @return a high quality random number in the range specified
 * @see secRandom(uint32_t, uint32_t)
 */
 uint8_t secRandom8(uint8_t minLim, uint8_t maxLim) {
    return secRandom(minLim, maxLim > 0 ? maxLim : UINT8_MAX);
}

/**
 * Leverages ECC608B's High-Quality NIST SP 800-90A/B/C Random Number Generator
 * <p>It is slow - takes about 30ms</p>
 * @param minLim minimum value, defaults to 0
 * @param maxLim maximum value, defaults to max unsigned 16 bits (UINT16_MAX)
 * @return a high quality random number in the range specified
 * @see secRandom(uint32_t, uint32_t)
 */
uint16_t secRandom16(const uint16_t minLim, const uint16_t maxLim) {
    return secRandom(minLim, maxLim > 0 ? maxLim : UINT16_MAX);
}

/**
 * Leverages ECC608B's High-Quality NIST SP 800-90A/B/C Random Number Generator
 * <p>It is slow - takes about 30ms</p>
 * @param minLim minimum value, defaults to 0
 * @param maxLim maximum value, defaults to max signed 32 bits (INT32_MAX)
 * @return a high quality random number in the range specified
 */
uint32_t secRandom(const uint32_t minLim, const uint32_t maxLim) {
    long low = (long)minLim;
    long high = maxLim > 0 ? (long)maxLim : INT32_MAX;
    return sysInfo->isSysStatus(SYS_STATUS_ECC) ? ECCX08.random(low, high) : random(low, high);
}

bool secElement_setup() {
    if (!ECCX08.begin()) {
        Log.error(F("No ECC608 chip present on the RP2040 board (or failed communication)!"));
        return false;
    }
    sysInfo->setSecureElementId(ECCX08.serialNumber());
    const char* eccSerial = sysInfo->getSecureElementId().c_str();
    if (!ECCX08.locked()) {
        Log.warn(F("The ECCX08 s/n %s on your board is not locked - proceeding with default TLS configuration locking."), eccSerial);
        if (!ECCX08.writeConfiguration(ECCX08_DEFAULT_TLS_CONFIG)) {
            Log.error(F("Writing ECCX08 default TLS configuration FAILED for s/n %s! Secure Element functions (RNG, etc.) NOT available"), eccSerial);
            return false;
        }
        if (!ECCX08.lock()) {
            Log.error(F("Locking ECCX08 configuration FAILED for s/n %s! Secure Element functions (RNG, etc.) NOT available"), eccSerial);
            return false;
        }
        Log.info(F("ECCX08 secure element s/n %s has been locked successfully!"), eccSerial);
    }
    Log.info(F("ECCX08 secure element OK! (s/n %s)"), eccSerial);
    sysInfo->setSysStatus(SYS_STATUS_ECC);
    //update entropy - the timing of this call allows us to interact with I2C without other contenders
    uint16_t rnd = secRandom16();
    random16_add_entropy(rnd);
    Log.info(F("Secure random value %hu added as entropy to pseudo random number generator"), rnd);
    return true;
}

/**
 * Setup the watchdog to 10 seconds - this is run once in the setup step at the program start
 * Take the opportunity and log if this program start is the result of a watchdog reboot
 */
void watchdogSetup() {
    if (watchdog_caused_reboot()) {
        time_t rebootTime = now();
        Log.warn(F("A watchdog caused reboot has occurred at %s"), StringUtils::asString(rebootTime).c_str());
        sysInfo->watchdogReboots().push(rebootTime);
        sysInfo->markDirtyBoot();
    }
    //if no ping in 3 seconds, reboot
    watchdog_enable(3000, true);
    //rp2040.wdt_begin(3000);
}

/**
 * Reset the watchdog
 */
void watchdogPing() {
    watchdog_update();
    //rp2040.wdt_reset();
}

/**
 * Delays a task for number of ms provided by leveraging FreeRTOS task primitives. Alternate to the plain delay() function that may be implemented
 * differently on FastLED, or collide with the RP2040 port implementation
 * @param ms number of milliseconds to delay
 */
void taskDelay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * Retrieves the current runtime counter value used for task monitoring and profiling.
 * The value is derived by converting the tick count to milliseconds.
 *
 * @return the runtime counter value in milliseconds, representing the time since the system start-up.
 */
unsigned long ulMainGetRunTimeCounterValue() {
    return xTaskGetTickCount()/pdMS_TO_TICKS(1);
}

