// Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef FS_SHA256_H
#define FS_SHA256_H

#include <Arduino.h>
#include <pico/sha256.h>

// Function to calculate SHA-256 using BearSSL directly
String sha256(const uint8_t *data, size_t len);
String sha256(const String &data);

pico_sha256_state_t* sha256_init();
void sha256_update(pico_sha256_state_t *ctx, const uint8_t *data, size_t len);
String sha256_final(pico_sha256_state_t *ctx);

#endif //SHA256_H
