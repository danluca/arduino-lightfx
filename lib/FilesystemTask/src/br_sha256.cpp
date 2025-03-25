// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "br_sha256.h"

/**
 * Convenience for converting byte array to hex - Same as StringUtils::asHexString
 * @param data data
 * @param len size of data
 * @return data as a hex string
 */
String byteArrayToHex(const uint8_t *data, const size_t len) {
    String hexString;
    hexString.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        char buffer[3];
        snprintf(buffer, sizeof(buffer), "%02x", data[i]);
        hexString += buffer;
    }
    return hexString;
}

/**
 * Computes SHA-256 hash value of the input data
 * @param data data
 * @param len size of data
 * @return SHA-256 of data as hex string
 */
String sha256(const uint8_t *data, const size_t len) {
    // BearSSL context structure for SHA-256
    br_sha256_context ctx;
    uint8_t hash[32]; // SHA-256 produces 32-byte output

    // Initialize the SHA-256 context
    br_sha256_init(&ctx);

    // Update the context with the input data
    br_sha256_update(&ctx, data, len);

    // Finalize the hash (writes the digest into the `hash` buffer)
    br_sha256_out(&ctx, hash);

    return byteArrayToHex(hash, sizeof(hash));
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
 * @return BearSSL SHA-256 context created
 */
br_sha256_context * sha256_init() {
    const auto ctx = new br_sha256_context;
    br_sha256_init(ctx);
    return ctx;
}

/**
 * Updates the SHA-256 context with a new data block. This method can be called repeatedly to calculate
 * the SHA-256 value of a larger data (e.g. buffered file read)
 * @param ctx the BearSSL SHA-256 context initialized by sha256_init
 * @param data data block
 * @param len size of data block
 */
void sha256_update(br_sha256_context *ctx, const uint8_t *data, const size_t len) {
    br_sha256_update(ctx, data, len);
}

/**
 * Finishes the SHA-256 hash calculations and returns the result. Needs at least one call to sha256_update
 * @param ctx the BearSSL SHA-256 context initialized by sha256_init
 * @return SHA-256 value as hex string
 */
String sha256_final(br_sha256_context *ctx) {
    uint8_t hash[32];
    br_sha256_out(ctx, hash);
    delete ctx;
    return byteArrayToHex(hash, sizeof(hash));
}
