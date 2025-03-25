// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef BR_SHA256_H
#define BR_SHA256_H

#include <bearssl/bearssl.h>
#include <Arduino.h>

// Function to calculate SHA-256 using BearSSL directly
String sha256(const uint8_t *data, size_t len);
String sha256(const String &data);

br_sha256_context* sha256_init();
void sha256_update(br_sha256_context *ctx, const uint8_t *data, size_t len);
String sha256_final(const br_sha256_context *ctx);

#endif //SHA256_H
