// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_CIRCULAR_BUFFER_H
#define ARDUINO_LIGHTFX_CIRCULAR_BUFFER_H

#include <vector>
#include <rtos.h>
//#include <mutex>

using namespace rtos;

/**
 * @class CircularBuffer
 *
 * @brief A template class that represents a circular buffer.
 *
 * A circular buffer is a data structure that allows sequential access to a fixed-size buffer.
 * This class provides thread-safe methods to push values to the buffer and pop values from the buffer.
 *
 * @tparam T The type of elements stored in the buffer.
 */
template<typename T> class CircularBuffer {
public:
    explicit CircularBuffer(size_t size) : buffer_(size), head_(0), tail_(0), full_(false), mutex_() {}

    void push_back(T value) {
        ScopedMutexLock lock(mutex_);
        buffer_[head_] = value;
        if(full_)
            tail_ = (tail_ + 1) % buffer_.size();
        head_ = (head_ + 1) % buffer_.size();
        full_ = head_ == tail_;
    }

    void push_back(T value[], size_t sz) {
        ScopedMutexLock lock(mutex_);
        for (size_t i = 0; i < sz; i++) {
            buffer_[head_] = value[i];
            if(full_)
                tail_ = (tail_ + 1) % buffer_.size();
            head_ = (head_ + 1) % buffer_.size();
            full_ = head_ == tail_;
        }
    }

    T pop_front() {
        ScopedMutexLock lock(mutex_);
        if(empty())
            return T();

        auto val = buffer_[tail_];
        full_ = false;
        tail_ = (tail_ + 1) % buffer_.size();

        return val;
    }

    size_t pop_front(T dest[], size_t sz) {
        ScopedMutexLock lock(mutex_);
        if (empty())
            return 0;
        size_t avail = min(sz, size());
        for (size_t i = 0; i < avail; i++) {
            dest[i] = buffer_[tail_];
            tail_ = (tail_ + 1) % buffer_.size();
        }
        full_ = false;
        return avail;
    }

    bool empty() const {
        return (!full_ && (head_ == tail_));
    }

    bool full() const {
        return full_;
    }

    size_t capacity() const {
        return buffer_.size();
    }

    size_t size() const {
        return full_ ? buffer_.size() : (head_ >= tail_ ? head_ - tail_ : buffer_.size() + head_ - tail_);
    }

    //implement iterator
//    class iterator {
//    public:
//        iterator(std::vector<T> &buffer, size_t index) : buffer_(buffer), index_(index) {}
//
//        iterator &operator++() {
//            index_ = (index_ + 1) % buffer_.size();
//            return *this;
//        }
//
//        iterator operator++(int) {
//            iterator temp = *this;
//            ++(*this);
//            return temp;
//        }
//
//        bool operator==(const iterator &other) const {
//            return index_ == other.index_;
//        }
//
//        bool operator!=(const iterator &other) const {
//            return !(*this == other);
//        }
//
//        T &operator*() {
//            return buffer_[index_];
//        }
//
//    private:
//        std::vector<T> &buffer_;
//        size_t index_;
//    };
//
//    iterator begin() {
//        return iterator(buffer_, tail_);
//    }
//
//    iterator end() {
//        return iterator(buffer_, head_);
//    }

private:
    std::vector<T> buffer_;
    size_t head_;
    size_t tail_;
    bool full_;
    Mutex mutex_;
};

#endif //ARDUINO_LIGHTFX_CIRCULAR_BUFFER_H
