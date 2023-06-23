//
// Created by Dan on 06.21.2023.
//

#ifndef LIGHTFX_FXD_H
#define LIGHTFX_FXD_H

#include "efx_setup.h"

// Define variables used by the sequences.
extern uint8_t   thisfade;
extern int       thishue;
extern uint8_t   thisinc;
extern uint8_t   thissat;
extern uint8_t   thisbri;
extern int       huediff;
extern uint8_t   thisdelay;
extern uint8_t dotBpm;
extern uint8_t fadeval;


void confetti();
void d02_ChangeMe();
void dot_beat();

class FxD1 : public LedEffect {
public:
    FxD1();

    void setup() override;
    void loop() override;
    const char *description() override;
};

class FxD2 : public LedEffect {
public:
    FxD2();

    void setup() override;
    void loop() override;
    const char *description() override;
};


#endif //LIGHTFX_FXD_H
