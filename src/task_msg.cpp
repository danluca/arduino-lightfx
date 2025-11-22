// MIT License
//
// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "task_msg.h"


/**
 * @brief Initializes the communication queues used for task interactions.
 *
 * This function sets up multiple FreeRTOS queues that facilitate message passing and action handling
 * between various tasks in the system. The queues are used for specific task-related operations,
 * including alarm management, broadcast messages, diagnostics, FX actions, and microphone-related actions.
 *
 * The function creates the following queues:
 * - `almQueue`: A queue for ALM-related tasks to handle communication between cores. Stores `AlmAction` items.
 * - `bcQueue`: A broadcast queue to manage actions sent by enqueue methods and processed by the execute method.
 *   Stores pointers to `bcTaskMessage` structures.
 * - `diagQueue`: A queue for diagnostic actions, storing `DiagAction` items.
 * - `fxQueue`: A queue for FX-related actions, storing `FxActionMessage` items.
 */
void task_msg_setup() {
    //create a receiving queue for the ALM task for communication between cores
    almQueue = xQueueCreate(10, sizeof(AlmAction));

    // create the broadcast queue, used by enqueue methods to send actions and execute method to receive and execute actions
    // stores pointers to bcTaskMessage allocated by producers; consumer deletes after processing
    bcQueue = xQueueCreate(10, sizeof(bcTaskMessage*));

    diagQueue = xQueueCreate(20, sizeof(DiagAction));

    fxQueue = xQueueCreate(10, sizeof(FxActionMessage));
}
