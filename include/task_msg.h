// MIT License
//
// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef RP2040_LIGHTFX_TASK_MSG_H
#define RP2040_LIGHTFX_TASK_MSG_H

#include <FreeRTOS.h>
#include <queue.h>

extern QueueHandle_t fxQueue;
extern QueueHandle_t almQueue;
extern QueueHandle_t bcQueue;
extern QueueHandle_t diagQueue;

enum AlmAction:uint8_t {ALARM_SETUP, ALARM_CHECK, SAVE_SYS_INFO, HOLIDAY_UPDATE};

enum FxAction:uint8_t {AUTO_FX, MANUAL_FX, COLOR_THEME, STRIP_BRIGHTNESS, AUDIO_THRESHOLD, SLEEP_ENABLED, AUDIO_CHANGE};
struct FxActionMessage {
    FxAction action;
    uint32_t data;
};

enum CommAction:uint8_t {TIME_SETUP, TIME_UPDATE, FX_SYNC, WIFI_ENSURE, STATUS_LED_CHECK, ENABLE_BROADCAST};

/**
 * Structure of the message sent to the Communications task
 */
struct bcTaskMessage {
    CommAction event;
    uint16_t data;
};

enum DiagAction:uint8_t {RND_ENTROPY, SYS_TEMP, SYS_VOLTAGE, DIAG_INFO, RESET_CALIBRATION};

enum MikeAction:uint8_t {AUDIO_THRESHOLD_UPDATE};
struct AudioActionMessage {
    MikeAction action;
    uint32_t data;
};

void task_msg_setup();

#endif //RP2040_LIGHTFX_TASK_MSG_H