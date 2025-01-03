// Copyright (c) 2024 by Dan Luca. All rights reserved.
//

#include "LogQueue.h"

using namespace LogUtil;

/**
 * Constructor for the LogQueue class.
 * Initializes the member variables including the mutex.
 */
LogQueue::LogQueue():m_mutex() {
}

/**
 *
 * @param message
 */
void LogQueue::push(const String* message) {
    // Lock the mutex to ensure thread-safety
    CoreMutex lock(&m_mutex);
    m_queue.push_back(message);
}

/**
 *
 * @return
 */
const String *LogQueue::pop() {
    CoreMutex lock(&m_mutex);
    // Pop the front message - do not call this when queue is empty!
    const String *message = m_queue.front();
    m_queue.pop_front();
    return message;
}

/**
 *
 * @return
 */
bool LogQueue::empty() const {
    CoreMutex lock(&m_mutex);
    return m_queue.empty();
}

/**
 *
 * @return
 */
size_t LogQueue::size() const {
    return m_queue.size();
}

/**
 *
 * @return
 */
size_t LogQueue::memorySize() const {
    CoreMutex lock(&m_mutex);
    if (m_queue.empty())
        return 0;
    size_t size = 0;
    for (const auto s : m_queue)
        size += s->length()+1;    //account for null terminator
    return size;
}

