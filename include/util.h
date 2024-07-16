//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved.
//

#ifndef ARDUINO_LIGHTFX_UTIL_H
#define ARDUINO_LIGHTFX_UTIL_H

#include <Arduino.h>
#include <Arduino_LSM6DSOX.h>
#include <ArduinoECCX08.h>
#include <queue>
#include <deque>
#include <vector>
#include <mutex>
#include "timeutil.h"
#include "config.h"
#include "secrets.h"
#include "log.h"

#define IMU_TEMPERATURE_NOT_AVAILABLE   0.001f
#define TEMP_NA_COMPARE_EPSILON      0.000001f

#define SYS_STATUS_WIFI    0x01
#define SYS_STATUS_NTP     0x02
#define SYS_STATUS_DST     0x04
#define SYS_STATUS_MIC     0x08
#define SYS_STATUS_FILESYSTEM     0x10
#define SYS_STATUS_ECC     0x20

extern const char stateFileName[];

float boardTemperature(bool bFahrenheit = false);
float chipTemperature(bool bFahrenheit = false);
inline static float toFahrenheit(float celsius) {
    return celsius * 9.0f / 5 + 32;
}

float controllerVoltage();
ulong adcRandom();
void setupStateLED();
void updateStateLED(uint32_t colorCode);
void updateStateLED(uint8_t red, uint8_t green, uint8_t blue);

uint8_t bmul8(uint8_t a, uint8_t b);
uint8_t bscr8(uint8_t a, uint8_t b);
uint8_t bovl8(uint8_t a, uint8_t b);
bool rblend8(uint8_t &a, uint8_t b, uint8_t amt=22) ;

void fsInit();

size_t readTextFile(const char *fname, String *s);
size_t writeTextFile(const char *fname, String *s);
bool removeFile(const char *fname);

const uint8_t setSysStatus(uint8_t bitMask);
const uint8_t resetSysStatus(uint8_t bitMask);
bool isSysStatus(uint8_t bitMask);
const uint8_t getSysStatus();

uint8_t secRandom8(uint8_t minLim = 0, uint8_t maxLim = 0);
uint16_t secRandom16(uint16_t minLim = 0, uint16_t maxLim = 0);
uint32_t secRandom(uint32_t minLim = 0, uint32_t maxLim = 0);
bool secElement_setup();

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

extern FixedQueue<TimeSync, 8> timeSyncs;

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
        rtos::ScopedMutexLock lock(mutex_);
        buffer_[head_] = value;
        if(full_)
            tail_ = (tail_ + 1) % buffer_.size();
        head_ = (head_ + 1) % buffer_.size();
        full_ = head_ == tail_;
    }

    void push_back(T value[], size_t sz) {
        rtos::ScopedMutexLock lock(mutex_);
        for (size_t i = 0; i < sz; i++) {
            buffer_[head_] = value[i];
            head_ = (head_ + 1) % buffer_.size();
        }
        if (full_)
            tail_ = (tail_ + sz) % buffer_.size();
        full_ = head_ == tail_;
    }

    T pop_front() {
        rtos::ScopedMutexLock lock(mutex_);
        if(empty())
            return T();

        auto val = buffer_[tail_];
        full_ = false;
        tail_ = (tail_ + 1) % buffer_.size();

        return val;
    }

    size_t pop_front(T dest[], size_t sz) {
        rtos::ScopedMutexLock lock(mutex_);
        if (empty())
            return 0;
        size_t avail = min(sz, size());
        for (size_t i = 0; i < avail; i++) {
            dest[i] = buffer_[tail_];
            tail_ = (tail_ + 1) % buffer_.size();
        }
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
    rtos::Mutex mutex_;
};

extern CircularBuffer<short> *audioData;
#endif //ARDUINO_LIGHTFX_UTIL_H
