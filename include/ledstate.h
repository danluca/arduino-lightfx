// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#ifndef LEDSTATE_H
#define LEDSTATE_H

void setupStateLED();
void updateStateLED(uint32_t colorCode);
void updateStateLED(uint8_t red, uint8_t green, uint8_t blue);

#endif //LEDSTATE_H
