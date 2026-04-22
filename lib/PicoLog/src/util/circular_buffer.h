// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef PICO_LOG_CIRCULAR_BUFFER_H
#define PICO_LOG_CIRCULAR_BUFFER_H

#include <Arduino.h>
#include <vector>
namespace LogUtil {
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
        explicit CircularBuffer(size_t size) : buffer_(size), head_(0), tail_(0), full_(false), mutex_() {
        }
        void push_back(const T value) {
            CoreMutex coreMutex(&mutex_);
            buffer_[head_] = value;
            if(full_)
                tail_ = (tail_ + 1) % buffer_.size();
            head_ = (head_ + 1) % buffer_.size();
            full_ = head_ == tail_;
        }

        void push_back(const T value[], const size_t sz) {
            CoreMutex coreMutex(&mutex_);
            size_t i = sz > buffer_.size() ? sz - buffer_.size() : 0;
            for (; i < sz; ++i) {
                buffer_[head_] = value[i];
                if(full_)
                    tail_ = (tail_ + 1) % buffer_.size();
                head_ = (head_ + 1) % buffer_.size();
                full_ = head_ == tail_;
            }
        }

        T pop_front() {
            CoreMutex coreMutex(&mutex_);
            if(_empty())
                return T();

            auto val = buffer_[tail_];
            full_ = false;
            tail_ = (tail_ + 1) % buffer_.size();

            return val;
        }

        size_t pop_front(T dest[], const size_t sz) {
            CoreMutex coreMutex(&mutex_);
            if (_empty())
                return 0;
            const size_t avail = min(sz, _size());
            for (size_t i = 0; i < avail; i++) {
                dest[i] = buffer_[tail_];
                tail_ = (tail_ + 1) % buffer_.size();
            }
            full_ = false;
            return avail;
        }

        void clear() {
            CoreMutex coreMutex(&mutex_);
            head_ = 0;
            tail_ = 0;
            full_ = false;
        }

        [[nodiscard]] bool empty() const {
            CoreMutex coreMutex(const_cast<mutex_t*>(&mutex_));
            return _empty();
        }

        [[nodiscard]] bool full() const {
            return full_;
        }

        [[nodiscard]] size_t capacity() const {
            return buffer_.size();
        }

        [[nodiscard]] size_t size() const {
            CoreMutex coreMutex(const_cast<mutex_t*>(&mutex_));
            return _size();
        }

    private:
        std::vector<T> buffer_;
        size_t head_;
        size_t tail_;
        bool full_;
        mutable mutex_t mutex_;

        [[nodiscard]] bool _empty() const {
            return (!full_ && (head_ == tail_));
        }

        [[nodiscard]] size_t _size() const {
            return full_ ? buffer_.size() : (head_ >= tail_ ? head_ - tail_ : buffer_.size() + head_ - tail_);
        }
    };
}
#endif //PICO_LOG_CIRCULAR_BUFFER_H
