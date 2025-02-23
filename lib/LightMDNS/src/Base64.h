// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#ifndef BASE64_H
#define BASE64_H
#include <Arduino.h>
//#include "../libraries/HTTPClient/src/base64.h" //TODO: instead of our own?

class Base64 {
    static constexpr const char encodingTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static constexpr size_t BITS_PER_CHAR = 6;        // Base64 uses 6 bits per character
    static constexpr size_t BITS_PER_BYTE = 8;        // Input uses 8 bits per byte
    static constexpr size_t OUTPUT_GROUP_SIZE = 4;    // Output characters per group
    static constexpr size_t INPUT_GROUP_SIZE = 3;     // Input bytes per group
    static constexpr char PADDING_CHAR = '=';         // Character used for padding
    static constexpr uint8_t MASK_6BITS = 0x3F;       // Mask for 6 bits (2^6 - 1)
    static constexpr uint8_t MASK_4BITS = 0x0F;       // Mask for 4 bits (2^4 - 1)
    static constexpr uint8_t MASK_2BITS = 0x03;       // Mask for 2 bits (2^2 - 1)
    static constexpr uint8_t MASK_6BITS_NOT = 0xC0;
    static constexpr uint8_t MASK_4BITS_NOT = 0xF0;

public:
    static size_t length(const size_t inputLength) {
        return OUTPUT_GROUP_SIZE * ((inputLength + INPUT_GROUP_SIZE - 1) / INPUT_GROUP_SIZE);
    }
    static size_t encode(const uint8_t* input, const size_t inputLength, char* output, const size_t outputLength) {
        if (outputLength < OUTPUT_GROUP_SIZE * ((inputLength + INPUT_GROUP_SIZE - 1) / INPUT_GROUP_SIZE))
            return 0;
        size_t inputIndex, outputIndex = 0;
        for (inputIndex = 0; inputIndex + INPUT_GROUP_SIZE - 1 < inputLength; inputIndex += INPUT_GROUP_SIZE) {
            output[outputIndex++] = encodingTable[((input[inputIndex + 0] >> 2) & MASK_6BITS)];
            output[outputIndex++] = encodingTable[((input[inputIndex + 0] & MASK_2BITS) << 4) | ((input[inputIndex + 1] & MASK_6BITS_NOT) >> 4)];
            output[outputIndex++] = encodingTable[((input[inputIndex + 1] & MASK_4BITS) << 2) | ((input[inputIndex + 2] & MASK_6BITS_NOT) >> 6)];
            output[outputIndex++] = encodingTable[((input[inputIndex + 2] & MASK_6BITS))];
        }
        if (inputIndex < inputLength) {
            output[outputIndex++] = encodingTable[(input[inputIndex] >> 2) & MASK_6BITS];
            if (inputIndex == (inputLength - 1)) {
                output[outputIndex++] = encodingTable[(input[inputIndex] & MASK_2BITS) << 4];
                output[outputIndex++] = PADDING_CHAR;
            } else {
                output[outputIndex++] = encodingTable[((input[inputIndex + 0] & MASK_2BITS) << 4) | ((input[inputIndex + 1] & MASK_6BITS_NOT) >> 4)];
                output[outputIndex++] = encodingTable[((input[inputIndex + 1] & MASK_4BITS) << 2)];
            }
            output[outputIndex++] = PADDING_CHAR;
        }
        output[outputIndex] = '\0';
        return outputIndex;
    }
};


#endif //BASE64_H
