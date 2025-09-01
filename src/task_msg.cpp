// MIT License
//
// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "task_msg.h"



void task_msg_setup() {
    almQueue = xQueueCreate(10, sizeof(AlmAction));    //create a receiving queue for the ALM task for communication between cores

    // create the broadcast queue, used by enqueue methods to send actions and execute method to receive and execute actions
    bcQueue = xQueueCreate(10, sizeof(bcTaskMessage*));

    diagQueue = xQueueCreate(20, sizeof(DiagAction));

    fxQueue = xQueueCreate(10, sizeof(FxActionMessage));

    micQueue = xQueueCreate(10, sizeof(MikeAction));
}
