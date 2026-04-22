// Copyright (c) 2024,2025,2026 by Dan Luca. All rights reserved.
//

#pragma once
#ifndef ARDUINO_LIGHTFX_FIXED_QUEUE_H
#define ARDUINO_LIGHTFX_FIXED_QUEUE_H

#include <deque>

/**
 * @class FixedQueue
 *
 * @brief A fixed-capacity FIFO queue. When full, the oldest element is evicted on push.
 *
 * Implemented via composition over std::deque — not inheriting std::queue — to avoid
 * the virtual-destructor and non-virtual-push pitfalls of STL container inheritance.
 *
 * Ownership: elements are destroyed normally when evicted or when the queue is destroyed.
 * Use std::unique_ptr<T> for heap-allocated elements that need automatic cleanup.
 *
 * @tparam T       The element type.
 * @tparam MaxSize The maximum number of elements (compile-time, must be > 0).
 * @tparam Container Underlying container (default: std::deque<T>).
 */
template <typename T, size_t MaxSize, typename Container = std::deque<T>>
class FixedQueue {
public:
    typedef typename Container::iterator iterator;
    typedef typename Container::const_iterator const_iterator;

    FixedQueue() = default;
    ~FixedQueue() = default;

    FixedQueue(const FixedQueue&) = default;
    FixedQueue& operator=(const FixedQueue&) = default;
    FixedQueue(FixedQueue&&) = default;
    FixedQueue& operator=(FixedQueue&&) = default;

    // Push an lvalue — evicts the oldest element if at capacity.
    void push(const T& value) {
        preparePush();
        c.push_back(value);
    }

    // Push an rvalue (move) — required for std::unique_ptr elements.
    void push(T&& value) {
        preparePush();
        c.push_back(std::move(value));
    }

    // In-place construction — also enforces the capacity limit.
    template<typename... Args>
    void emplace(Args&&... args) {
        preparePush();
        c.emplace_back(std::forward<Args>(args)...);
    }

    [[nodiscard]] T& front()             { return c.front(); }
    [[nodiscard]] const T& front() const { return c.front(); }
    [[nodiscard]] T& back()              { return c.back(); }
    [[nodiscard]] const T& back() const  { return c.back(); }

    [[nodiscard]] bool   empty() const { return c.empty(); }
    [[nodiscard]] size_t size()  const { return c.size(); }

    iterator begin() { return c.begin(); }
    iterator end()   { return c.end(); }
    const_iterator begin() const { return c.begin(); }
    const_iterator end()   const { return c.end(); }

    // Erase element at iterator position; returns iterator to the next element.
    // Element is destroyed normally (unique_ptr cleans up automatically).
    iterator erase(iterator it) { return c.erase(it); }

private:
    Container c{};

    void preparePush() {
        if (c.size() >= MaxSize)
            c.pop_front();
    }
};

#endif //ARDUINO_LIGHTFX_FIXED_QUEUE_H
