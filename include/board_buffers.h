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

/**
 * Buffer capacities required for dynamic allocation of LED and effect workspaces.
 */
struct RuntimeBufferSizes {
    size_t ledPixels;       ///< Number of pixels in primary LED strip buffer
    size_t framePixels;     ///< Number of pixels in animation frame buffer (frameSize * 4)
    size_t shuffleIndexes;  ///< Number of elements in random shuffle index array
    size_t scratchWords;    ///< Size in 16-bit words of general effect scratch workspace
};

/**
 * Non-owning typed view into a slice of the scratch workspace memory.
 * @tparam T element type (e.g. uint16_t or uint8_t)
 */
template <typename T> struct ScratchView {
    T *data{};      ///< Pointer to start of view memory
    size_t size{};  ///< Number of elements in view

    /**
     * Checks if the view points to valid allocated memory.
     * @return true if data pointer is non-null
     */
    explicit operator bool() const noexcept {
        return data != nullptr;
    }
};

/**
 * Computes runtime buffer sizes from a board configuration with overflow validation.
 * @param configuration board configuration profile to derive sizes from
 * @param[out] sizes structure populated with calculated buffer capacities
 * @return true if configuration is valid and sizes calculated without overflow, false otherwise
 */
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

/**
 * Custom deleter functor for malloc-backed std::unique_ptr arrays.
 * @tparam T element type
 */
template <typename T> struct MallocArrayDeleter {
    using FreeFunction = void (*)(void *);
    FreeFunction freeFunction = &std::free;

    void operator()(T *ptr) const noexcept {
        if (ptr != nullptr)
            freeFunction(ptr);
    }
};

/**
 * Unique pointer managing dynamically allocated arrays with custom free functions.
 * @tparam T element type
 */
template <typename T> using MallocArray = std::unique_ptr<T[], MallocArrayDeleter<T>>;

/**
 * Allocates a zero-initialized array using custom allocator functions and wraps it in a MallocArray.
 * @tparam T element type (must be trivially destructible)
 * @param count number of elements to allocate
 * @param allocateFunction allocation function pointer (e.g. std::malloc)
 * @param freeFunction deallocation function pointer (e.g. std::free)
 * @param[out] result unique pointer populated with allocated array on success
 * @return true if allocation succeeded, false on failure or overflow
 */
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

/**
 * Container managing ownership of all dynamically allocated runtime LED and workspace buffers.
 * @tparam Pixel LED pixel color representation type (e.g. CRGB)
 */
template <typename Pixel> class BoardBufferStorage {
public:
    using AllocateFunction = void *(*)(size_t);
    using FreeFunction = void (*)(void *);

    /**
     * Allocates all runtime buffers based on the provided board configuration.
     * Guarantees atomic all-or-nothing allocation (rolls back on partial failure).
     * @param configuration board configuration determining buffer sizes
     * @param allocateFunction custom allocator function pointer (defaults to std::malloc)
     * @param freeFunction custom deallocator function pointer (defaults to std::free)
     * @return true if all buffers allocated successfully, false on error
     */
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

    /**
     * Obtains a bounds-checked typed 16-bit word view into a portion of the scratch buffer.
     * @param offset word offset from start of scratch buffer
     * @param count number of 16-bit words requested
     * @return ScratchView with valid pointer and size if within bounds, or empty view on error
     */
    [[nodiscard]] ScratchView<uint16_t> scratchWordView(const size_t offset, const size_t count) const {
        if (scratch == nullptr || offset > sizes.scratchWords || count > sizes.scratchWords - offset)
            return {};
        return {scratch.get() + offset, count};
    }

    /**
     * Obtains a bounds-checked typed 8-bit byte view into a portion of the scratch buffer.
     * @param offset byte offset from start of scratch buffer
     * @param count number of bytes requested
     * @return ScratchView with valid pointer and size if within bounds, or empty view on error
     */
    [[nodiscard]] ScratchView<uint8_t> scratchByteView(const size_t offset, const size_t count) const {
        if (scratch == nullptr || sizes.scratchWords > std::numeric_limits<size_t>::max() / sizeof(uint16_t))
            return {};
        const size_t capacityBytes = sizes.scratchWords * sizeof(uint16_t);
        if (offset > capacityBytes || count > capacityBytes - offset)
            return {};
        return {reinterpret_cast<uint8_t *>(scratch.get()) + offset, count};
    }

    /**
     * Swaps buffer pointers and size descriptors with another storage instance.
     * @param other instance to swap with
     */
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

    MallocArray<Pixel> leds;                ///< Primary LED strip pixel buffer
    MallocArray<Pixel> frame;               ///< Multi-frame animation rendering buffer
    MallocArray<uint16_t> shuffleIndexes;   ///< Random shuffle permutation index array
    MallocArray<uint16_t> scratch;          ///< Multi-use 16-bit word scratch workspace buffer
    RuntimeBufferSizes sizes{};             ///< Capacities of allocated buffers
};