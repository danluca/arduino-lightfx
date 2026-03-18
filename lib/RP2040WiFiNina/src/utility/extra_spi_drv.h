// MIT License
//
// Copyright (c) 2026 by Dan Luca. All rights reserved.
//

#ifndef RP2040_LIGHTFX_EXTRA_SPI_DRV_H
#define RP2040_LIGHTFX_EXTRA_SPI_DRV_H

#include "spi_drv.h"

class ExtraSpiDrv
{
public:
	static int waitResponseParams(uint8_t cmd, uint8_t numParam, tDataParam* params);
};

#endif //RP2040_LIGHTFX_EXTRA_SPI_DRV_H