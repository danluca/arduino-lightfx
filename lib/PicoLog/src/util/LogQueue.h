// Copyright (c) 2024 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef LOGQUEUE_H
#define LOGQUEUE_H

#include <Arduino.h>
#include <deque>

namespace LogUtil {
    class LogQueue final {
    public:
        explicit LogQueue();
        void push(const String* message);

        const String *pop();
        bool empty() const;
        size_t size() const;
        size_t memorySize() const;
        std::deque<const String *>::const_iterator begin() const { return m_queue.cbegin(); };
        std::deque<const String *>::const_iterator end() const { return m_queue.cend(); };
        std::deque<const String *>::const_reverse_iterator rbegin() const { return m_queue.crbegin(); };
        std::deque<const String *>::const_reverse_iterator rend() const { return m_queue.crend(); };

        ~LogQueue() = default;

    private:
        mutable mutex_t m_mutex; // Mutex to protect the queue
        std::deque<const String*> m_queue; // Queue to store log messages
    };
}
#endif //LOGQUEUE_H
