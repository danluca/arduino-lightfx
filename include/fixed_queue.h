// Copyright (c) 2024 by Dan Luca. All rights reserved.
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
    void push(const T& value) {
        if (this->size() == MaxSize)
            this->c.pop_front();
        std::queue<T, Container>::push(value);
    };
    typedef typename Container::iterator iterator;
    typedef typename Container::const_iterator const_iterator;

    iterator begin() { return this->c.begin(); }
    iterator end() { return this->c.end(); }
    const_iterator begin() const { return this->c.begin(); }
    const_iterator end() const { return this->c.end(); }
};

#endif //ARDUINO_LIGHTFX_FIXED_QUEUE_H
