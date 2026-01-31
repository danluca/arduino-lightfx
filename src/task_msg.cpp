// MIT License
//
// Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
//

#include <Arduino.h>
#include "task_msg.h"
#include "constants.hpp"
#include "log.h"

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
    if (almQueue == nullptr) {
        log_error(F("Failed to create almQueue - ALM communication will not work"));
    }

    // create the broadcast queue, used by enqueue methods to send actions and execute method to receive and execute actions
    // stores pointers to bcTaskMessage allocated by producers; consumer deletes after processing
    // Queue size increased from 10 to 20 to handle higher message throughput during network activity
    bcQueue = xQueueCreate(20, sizeof(bcTaskMessage*));
    if (bcQueue == nullptr) {
        log_error(F("Failed to create bcQueue - broadcast communication will not work"));
    }

    diagQueue = xQueueCreate(20, sizeof(DiagAction));
    if (diagQueue == nullptr) {
        log_error(F("Failed to create diagQueue - diagnostic messaging will not work"));
    }

    fxQueue = xQueueCreate(10, sizeof(FxActionMessage));
    if (fxQueue == nullptr) {
        log_error(F("Failed to create fxQueue - FX messaging will not work"));
    }
}

/**
 * @brief Safe way to retrieve timer ID from a FreeRTOS timer handle
 *
 * Retrieves the timer ID from a FreeRTOS timer handle in a null-safe manner.
 *
 * Assumption: the timer ID set during xTimerCreate creation or explicitly with vTimerSetTimerID is a 2-byte unsigned integer `uint16_t`
 * @param timer timer handle to retrieve ID from
 * @return timer id or 0 if timer is null
 */
uint16_t getTimerId(const TimerHandle_t timer) {
    if (!timer) return 0;
    const auto timerId = static_cast<uint16_t *>(pvTimerGetTimerID(timer));
    return timerId ? *timerId : 0;
}

/**
 * @brief Safe way to retrieve the timer name from a FreeRTOS timer handle
 *
 * Retrieves the timer name from a FreeRTOS timer handle in a null-safe manner.
 *
 * Assumption: the timer name is a null-terminated string
 * @param timer timer handle to retrieve name from
 * @return timer name or 'N/R' if timer is null
 */
const char * getTimerName(const TimerHandle_t timer) {
    if (!timer) return ::strNR;
    const auto timerName = pcTimerGetName(timer);
    return timerName ? timerName : strNR;
}
