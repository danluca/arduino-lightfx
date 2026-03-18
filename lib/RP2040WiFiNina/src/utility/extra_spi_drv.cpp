// MIT License
//
// Copyright (c) 2026 by Dan Luca. All rights reserved.
//

#include <Arduino.h>
#include "extra_spi_drv.h"
#include "utility/debug.h"

#define WAIT_START_CMD(x) SpiDrv::waitSpiChar(START_CMD)

#define IF_CHECK_START_CMD(x) \
if (!WAIT_START_CMD(_data)) { \
TOGGLE_TRIGGER() \
WARN("Error waiting START_CMD"); \
return 0; \
} else

#define CHECK_DATA(check, x) \
if (!SpiDrv::readAndCheckChar(check, &x)) { \
TOGGLE_TRIGGER() \
WARN("Reply error"); \
INFO2(check, (uint8_t)x); \
return 0; \
} else


int ExtraSpiDrv::waitResponseParams(const uint8_t cmd, const uint8_t numParam, tDataParam *params) {
    char _data = 0;
    int i = 0, ii = 0;


    IF_CHECK_START_CMD(_data) {
        CHECK_DATA(cmd | REPLY_FLAG, _data) {
        };

        uint8_t _numParam = SpiDrv::readChar();
        if (_numParam != 0) {
            for (i = 0; i < _numParam; ++i) {
                params[i].dataLen = SpiDrv::readParamLen16();
                for (ii = 0; ii < params[i].dataLen; ++ii) {
                    // Get Params data
                    params[i].data[ii] = SpiDrv::spiTransfer(DUMMY_DATA);
                }
            }
        } else {
            WARN("Error numParam == 0");
            return 0;
        }

        if (numParam != _numParam) {
            WARN("Mismatch numParam");
            return 0;
        }

        SpiDrv::readAndCheckChar(END_CMD, &_data);
    }
    return 1;
}
