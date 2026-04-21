// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_CIRCULAR_BUFFER_H
#define ARDUINO_LIGHTFX_CIRCULAR_BUFFER_H

#include <Arduino.h>
#include <vector>

/**
 * @class CircularBuffer
 *
 * @brief A thread-safe circular buffer with fixed capacity.
 *
 * Individual push, pop, and clear operations are each fully mutex-protected.
 * The query methods empty(), full(), size(), and capacity() are also mutex-protected
 * (except capacity(), which is immutable after construction).
 *
 * Iterators are NOT thread-safe — they snapshot head/tail at construction time
 * and hold no lock during traversal. Use iterators only from a single-producer context
 * or with external synchronization.
 *
 * @tparam T The type of elements stored in the buffer.
 * @see https://github.com/ARMmbed/mbed-os/blob/master/platform/include/platform/CircularBuffer.h
 */
template<typename T>
class CircularBuffer {
public:
    explicit CircularBuffer(size_t size) : buffer_(size), head_(0), tail_(0), full_(false), mutex_() {
    }
    ~CircularBuffer() { clear(); }

    void push_back(const T value) {
        CoreMutex coreMutex(&mutex_);
        if (full_) {
            if constexpr (std::is_pointer_v<T>)
                delete buffer_[head_];
            tail_ = (tail_ + 1) % buffer_.size();
        }
        buffer_[head_] = std::move(value);
        head_ = (head_ + 1) % buffer_.size();
        full_ = head_ == tail_;
    }

    void push_back(const T value[], const size_t sz) {
        CoreMutex coreMutex(&mutex_);
        size_t i = sz > capacity() ? sz - capacity() : 0;
        for (; i < sz; ++i) {
            if (full_) {
                if constexpr (std::is_pointer_v<T>)
                    delete buffer_[head_];
                tail_ = (tail_ + 1) % buffer_.size();
            }
            buffer_[head_] = std::move(value[i]);
            head_ = (head_ + 1) % buffer_.size();
            full_ = head_ == tail_;
        }
    }

    bool pop_front(T& result) {
        CoreMutex coreMutex(&mutex_);
        if (_empty()) return false;
        result = std::move(buffer_[tail_]);
        full_ = false;
        tail_ = (tail_ + 1) % buffer_.size();
        return true;
    }

    T pop_front() {
        CoreMutex coreMutex(&mutex_);
        if (_empty())
            return T();
        auto val = std::move(buffer_[tail_]);
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
            dest[i] = std::move(buffer_[tail_]);
            tail_ = (tail_ + 1) % buffer_.size();
        }
        if (avail > 0) full_ = false;   // don't clear full_ if nothing was actually removed
        return avail;
    }

    void clear() {
        CoreMutex coreMutex(&mutex_);
        if constexpr (std::is_pointer_v<T>) {
            // full_ is updated inside the loop so _empty() terminates correctly even
            // when starting from a full buffer (where head_ == tail_ would otherwise
            // keep _empty() returning false indefinitely).
            while (!_empty()) {
                delete buffer_[tail_];
                tail_ = (tail_ + 1) % buffer_.size();
                full_ = false;
            }
        }
        head_ = 0; tail_ = 0; full_ = false;
    }

    // Thread-safe query methods. Each acquires the mutex independently — do not
    // call from a method that already holds mutex_ (use the private _empty/_size/_full instead).
    [[nodiscard]] bool empty() const {
        CoreMutex coreMutex(&mutex_);
        return _empty();
    }
    [[nodiscard]] bool full() const {
        CoreMutex coreMutex(&mutex_);
        return _full();
    }
    // capacity() needs no lock: the buffer vector size is fixed at construction and never changes.
    [[nodiscard]] size_t capacity() const { return buffer_.size(); }
    [[nodiscard]] size_t size() const {
        CoreMutex coreMutex(&mutex_);
        return _size();
    }

    // --- Iterator Implementation ---
    // WARNING: iterators are NOT thread-safe. begin()/end() read head_/tail_ without a lock.
    // Any concurrent push/pop/clear during traversal is a data race.
    template <typename ValueType> class IteratorBase {
    public:
        IteratorBase(const CircularBuffer<T>* parent, const size_t index, const bool is_end) : parent_(parent), index_(index), is_end_(is_end) {}

        ValueType& operator*() const { return parent_->buffer_[index_]; }
        ValueType* operator->() const { return &parent_->buffer_[index_]; }

        IteratorBase& operator++() {
            index_ = (index_ + 1) % parent_->buffer_.size();
            if (index_ == parent_->head_) is_end_ = true;
            return *this;
        }

        bool operator==(const IteratorBase& other) const {
            return (is_end_ == other.is_end_) && (index_ == other.index_);
        }

        bool operator!=(const IteratorBase& other) const { return !(*this == other); }

    private:
        const CircularBuffer<T>* parent_;
        size_t index_;
        bool is_end_;
    };

    using iterator = IteratorBase<T>;
    using const_iterator = IteratorBase<const T>;

    iterator begin() { return iterator(this, tail_, _empty()); }
    iterator end()   { return iterator(this, head_, true); }

    const_iterator begin() const { return const_iterator(this, tail_, _empty()); }
    const_iterator end()   const { return const_iterator(this, head_, true); }

    const_iterator cbegin() const { return begin(); }
    const_iterator cend()   const { return end(); }

private:
    std::vector<T> buffer_{};
    size_t head_;
    size_t tail_;
    bool full_;
    mutable mutex_t mutex_;

    // Lock-free internal versions — only call these from methods that already hold mutex_.
    [[nodiscard]] bool   _empty() const { return !full_ && head_ == tail_; }
    [[nodiscard]] bool   _full()  const { return full_; }
    [[nodiscard]] size_t _size()  const {
        return full_ ? buffer_.size() : (head_ >= tail_ ? head_ - tail_ : buffer_.size() + head_ - tail_);
    }
};

#endif //ARDUINO_LIGHTFX_CIRCULAR_BUFFER_H
