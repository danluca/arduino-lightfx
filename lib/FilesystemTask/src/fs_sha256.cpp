// Copyright (c) 2025,2026 by Dan Luca. All rights reserved.
//

#include "fs_sha256.h"
#include "stringutils.h"

/**
 * Computes SHA-256 hash value of the input data
 * @param data data
 * @param len size of data
 * @return SHA-256 of data as hex string
 */
String sha256(const uint8_t *data, const size_t len) {
    // context structure for SHA-256
    pico_sha256_state_t ctx;

    // initialize the context
    if (pico_sha256_try_start(&ctx, SHA256_BIG_ENDIAN, true) == PICO_OK) {
        sha256_result_t result;
        // Update the context with the input data
        pico_sha256_update(&ctx, data, len);
        // Finalize the hash (writes the digest into the `result` buffer)
        pico_sha256_finish(&ctx, &result);
        // convert to hex string
        return StringUtils::asHexString(result.bytes, SHA256_RESULT_BYTES);
    }
    pico_sha256_cleanup(&ctx);  //the finish already unlocks, but just in case
    return {};

}

/**
 * Computes SHA-256 hash value of the input data
 * @param data data
 * @return SHA-256 of data as hex string
 */
String sha256(const String &data) {
    return sha256(reinterpret_cast<const uint8_t *>(data.c_str()), data.length());
}

/**
 * Initializes an SHA-256 context - prepares to compute SHA-256 hash values
 * @return SHA-256 context created, or nullptr if the hardware engine is unavailable
 */
pico_sha256_state_t * sha256_init() {
    const auto ctx = new pico_sha256_state_t;
    // if not successful in acquiring the context (and locking the SHA256 engine), free resources and return nullptr
    if (pico_sha256_try_start(ctx, SHA256_BIG_ENDIAN, true) != PICO_OK) {
        delete ctx;
        return nullptr;
    }
    return ctx;
}

/**
 * Updates the SHA-256 context with a new data block. This method can be called repeatedly to calculate
 * the SHA-256 value of a larger data (e.g. buffered file read)
 * @param ctx the SHA-256 context initialized by sha256_init
 * @param data data block
 * @param len size of data block
 */
void sha256_update(pico_sha256_state_t *ctx, const uint8_t *data, const size_t len) {
    if (ctx)
        pico_sha256_update_blocking(ctx, data, len);
}

/**
 * Finishes the SHA-256 hash calculations and returns the result. Needs at least one call to sha256_update
 * @param ctx the SHA-256 context initialized by sha256_init
 * @return SHA-256 value as hex string, or empty string if ctx is null
 */
String sha256_final(pico_sha256_state_t *ctx) {
    if (ctx == nullptr)
        return {};
    sha256_result_t hash;
    pico_sha256_finish(ctx, &hash);
    delete ctx;
    return StringUtils::asHexString(hash.bytes, SHA256_RESULT_BYTES);
}
