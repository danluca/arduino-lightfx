//
// Copyright by Dan Luca. All rights reserved
//
#pragma once

#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "board_config.h"

struct RuntimeBufferSizes {
    size_t ledPixels;
    size_t framePixels;
    size_t shuffleIndexes;
    size_t scratchWords;
};

template <typename T> struct ScratchView {
    T *data{};
    size_t size{};

    explicit operator bool() const noexcept {
        return data != nullptr;
    }
};

inline bool runtimeBufferSizes(const BoardConfiguration &configuration, RuntimeBufferSizes &sizes) {
    if (!isValidBoardConfiguration(configuration))
        return false;

    const size_t frameSize = configuration.frameSize;
    if (frameSize > std::numeric_limits<size_t>::max() / 4)
        return false;

    sizes = {
        configuration.numPixels,
        frameSize * 4,
        configuration.numPixels,
        configuration.numPixels
    };
    return true;
}

template <typename T> struct MallocArrayDeleter {
    using FreeFunction = void (*)(void *);
    FreeFunction freeFunction = &std::free;

    void operator()(T *ptr) const noexcept {
        if (ptr != nullptr)
            freeFunction(ptr);
    }
};

template <typename T> using MallocArray = std::unique_ptr<T[], MallocArrayDeleter<T>>;

template <typename T> bool allocateMallocArray(const size_t count, void *(*allocateFunction)(size_t), void (*freeFunction)(void *), MallocArray<T> &result) {
    static_assert(std::is_trivially_destructible<T>::value, "malloc-backed arrays must have trivial destructors");
    if (count == 0 || count > std::numeric_limits<size_t>::max() / sizeof(T))
        return false;

    void *memory = allocateFunction(count * sizeof(T));
    if (memory == nullptr)
        return false;

    MallocArray<T> candidate(static_cast<T *>(memory), MallocArrayDeleter<T>{freeFunction});
    for (size_t i = 0; i < count; ++i)
        ::new (static_cast<void *>(candidate.get() + i)) T{};
    result = std::move(candidate);
    return true;
}

template <typename Pixel> class BoardBufferStorage {
public:
    using AllocateFunction = void *(*)(size_t);
    using FreeFunction = void (*)(void *);

    bool allocate(const BoardConfiguration &configuration, AllocateFunction allocateFunction = &std::malloc, FreeFunction freeFunction = &std::free) {
        RuntimeBufferSizes newSizes{};
        if (allocateFunction == nullptr || freeFunction == nullptr || !runtimeBufferSizes(configuration, newSizes))
            return false;

        BoardBufferStorage candidate;
        if (!allocateMallocArray(newSizes.ledPixels, allocateFunction, freeFunction, candidate.leds) ||
            !allocateMallocArray(newSizes.framePixels, allocateFunction, freeFunction, candidate.frame) ||
            !allocateMallocArray(newSizes.shuffleIndexes, allocateFunction, freeFunction, candidate.shuffleIndexes) ||
            !allocateMallocArray(newSizes.scratchWords, allocateFunction, freeFunction, candidate.scratch))
            return false;

        candidate.sizes = newSizes;
        swap(candidate);
        return true;
    }

    [[nodiscard]] ScratchView<uint16_t> scratchWordView(const size_t offset, const size_t count) const {
        if (scratch == nullptr || offset > sizes.scratchWords || count > sizes.scratchWords - offset)
            return {};
        return {scratch.get() + offset, count};
    }

    [[nodiscard]] ScratchView<uint8_t> scratchByteView(const size_t offset, const size_t count) const {
        if (scratch == nullptr || sizes.scratchWords > std::numeric_limits<size_t>::max() / sizeof(uint16_t))
            return {};
        const size_t capacityBytes = sizes.scratchWords * sizeof(uint16_t);
        if (offset > capacityBytes || count > capacityBytes - offset)
            return {};
        return {reinterpret_cast<uint8_t *>(scratch.get()) + offset, count};
    }

    void swap(BoardBufferStorage &other) noexcept {
        leds.swap(other.leds);
        frame.swap(other.frame);
        shuffleIndexes.swap(other.shuffleIndexes);
        scratch.swap(other.scratch);
        std::swap(sizes.ledPixels, other.sizes.ledPixels);
        std::swap(sizes.framePixels, other.sizes.framePixels);
        std::swap(sizes.shuffleIndexes, other.sizes.shuffleIndexes);
        std::swap(sizes.scratchWords, other.sizes.scratchWords);
    }

    MallocArray<Pixel> leds;
    MallocArray<Pixel> frame;
    MallocArray<uint16_t> shuffleIndexes;
    MallocArray<uint16_t> scratch;
    RuntimeBufferSizes sizes{};
};