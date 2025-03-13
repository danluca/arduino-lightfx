//
// Copyright 2025 by Dan Luca. All rights reserved
//

#ifndef STRINGSTREAM_H
#define STRINGSTREAM_H

#include <Stream.h>

/**
 *  StringStream: A wrapper class to mimic Stream API for strings or char buffers
 */
class StringStream : public Stream {
public:
    virtual ~StringStream() = default;

    // Constructor for a C-style string buffer
    explicit StringStream(const char *buffer, size_t length);

    // Constructor for a String object
    explicit StringStream(const String &str);

    // Reading a single character
    int read() override;

    // Peek the next character without advancing the position
    int peek() override;

    // Check how much data is available
    int available() override;

    // Read characters into a buffer
    size_t readBytes(char *buffer, size_t length);

    // Flush (no-op here as there's no output buffer)
    void flush() override;

    // Write data (unsupported for StringStream, read-only stream)
    size_t write(uint8_t) override;

private:
    const char *_buffer;   // Pointer to the input string or char buffer
    size_t _length;        // Total length of the input
    size_t _position;      // Current reading position
};

#endif // STRINGSTREAM_H