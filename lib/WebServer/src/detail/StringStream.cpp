//
// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "StringStream.h"

StringStream::StringStream(const char *buffer, const size_t length) : _buffer(buffer), _length(length), _position(0) {
}

// Overload for string input
StringStream::StringStream(const String &str) : _buffer(str.c_str()), _length(str.length()), _position(0) {
}

// Reading a single character
int StringStream::read() override {
    if (_position < _length) {
        return _buffer[_position++];
    }
    return -1; // EOF
}

// Peek the next character without advancing the position
int StringStream::peek() override {
    if (_position < _length) {
        return _buffer[_position];
    }
    return -1; // EOF
}

// Check how much data is available
int StringStream::available() override {
    return _length - _position;
}

// Read characters into a buffer
size_t StringStream::readBytes(char *buffer, const size_t length) {
    size_t availableLength = _length - _position;
    size_t readLength = (length < availableLength) ? length : availableLength;
    memcpy(buffer, _buffer + _position, readLength);
    _position += readLength;
    return readLength;
}

// Flush (no-op here as there's no output buffer)
void StringStream::flush() override {
}

// Write data (unsupported for StringStream)
size_t StringStream::write(uint8_t) override {
    return 0; // Read-only stream
}
