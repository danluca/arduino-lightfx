// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_FIXED_QUEUE_H
#define ARDUINO_LIGHTFX_FIXED_QUEUE_H

#include <deque>
#include <queue>

/**
 * @class FixedQueue
 *
 * @brief A class that represents a fixed size queue.
 *
 * This class is a specialization of std::queue that limits the
 * maximum number of elements that can be stored in the queue to a
 * fixed number defined by the template parameter MaxSize. If the
 * queue exceeds its maximum size, the oldest element is automatically
 * removed when a new element is added.
 * @note If the type T is a raw pointer, the FixedQueue will take ownership of the pointers and delete them when they are removed from the queue.
 * @note The recommended way to use this queue with pointer types is to engage smart pointers (e.g., std::unique_ptr) to avoid manual memory management.
 *
 * @tparam T The type of elements to be stored in the queue.
 * @tparam MaxSize The maximum number of elements that can be stored in the queue.
 * @tparam Container The underlying container type used to store the elements (default: std::deque<T>).
 *
 * @note This class inherits from std::queue to provide the basic queue functionality.
 *
 * @see std::queue
 */
template <typename T, int MaxSize, typename Container = std::deque<T>> class FixedQueue : public std::queue<T, Container> {
public:
    FixedQueue() = default;
    ~FixedQueue() {
        clearAndCleanup();
    }
    // Handle lvalues (copies)
    void push(const T& value) {
        preparePush();
        std::queue<T, Container>::push(value);
    }
    // Handle rvalues (moves - required for std::unique_ptr)
    void push(T&& value) {
        preparePush();
        std::queue<T, Container>::push(std::move(value));
    }
    typedef typename Container::iterator iterator;
    typedef typename Container::const_iterator const_iterator;

    iterator begin() { return this->c.begin(); }
    iterator end() { return this->c.end(); }
    const_iterator begin() const { return this->c.begin(); }
    const_iterator end() const { return this->c.end(); }

    iterator erase(iterator it) {
        if constexpr (std::is_pointer_v<T>) {
            delete *it;
        }
        return this->c.erase(it);
    }

private:
    void preparePush() {
        if (this->size() >= MaxSize) {
            if constexpr (std::is_pointer_v<T>) {
                delete this->c.front();
            }
            this->c.pop_front();
        }
    }

    void clearAndCleanup() {
        if constexpr (std::is_pointer_v<T>) {
            while (!this->empty()) {
                delete this->c.front();
                this->c.pop_front();
            }
        }
    }
};

#endif //ARDUINO_LIGHTFX_FIXED_QUEUE_H
