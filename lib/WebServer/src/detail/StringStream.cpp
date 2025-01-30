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
int StringStream::read() {
    if (_position < _length) {
        return _buffer[_position++];
    }
    return -1; // EOF
}

// Peek the next character without advancing the position
int StringStream::peek() {
    if (_position < _length) {
        return _buffer[_position];
    }
    return -1; // EOF
}

// Check how much data is available
int StringStream::available() {
    return _length - _position;
}

// Read characters into a buffer
size_t StringStream::readBytes(char *buffer, const size_t length) {
    const size_t availableLength = _length - _position;
    const size_t readLength = (length < availableLength) ? length : availableLength;
    memcpy(buffer, _buffer + _position, readLength);
    _position += readLength;
    return readLength;
}

// Flush (no-op here as there's no output buffer)
void StringStream::flush() {
}

// Write data (unsupported for StringStream)
size_t StringStream::write(uint8_t) {
    return 0; // Read-only stream
}
