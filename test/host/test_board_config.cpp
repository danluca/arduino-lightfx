#include <cassert>
#include <cstddef>
#include <cstdlib>
#include "board_buffers.h"
#include "board_config.h"

static size_t allocationAttempts = 0;
static size_t liveAllocations = 0;
static size_t failAllocationAt = 0;

static void *testAllocate(const size_t size) {
    ++allocationAttempts;
    if (allocationAttempts == failAllocationAt)
        return nullptr;
    void *memory = std::malloc(size);
    if (memory != nullptr)
        ++liveAllocations;
    return memory;
}

static void testFree(void *memory) {
    if (memory != nullptr) {
        --liveAllocations;
        std::free(memory);
    }
}

int main() {
    size_t count = 0;
    const BoardConfiguration *configurations = knownBoardConfigurations(count);
    assert(configurations != nullptr);
    assert(count > 0);
    for (size_t i = 0; i < count; ++i)
        assert(isValidBoardConfiguration(configurations[i]));

    const BoardConfiguration *fx02 = findBoardConfiguration("E034C6063295681A");
    assert(fx02 != nullptr);
    assert(findBoardConfiguration("e034c6063295681a") == fx02);
    assert(fx02->numPixels == 170);
    assert(fx02->frameSize == 76);
    assert(fx02->useDhcp);
    assert(limitBrightness(*fx02, 200) == 200);
    assert(limitBrightness(*fx02, 255) == 255);
    assert(findBoardConfiguration(nullptr) == nullptr);
    assert(findBoardConfiguration("unknown") == nullptr);

    assert(selectBoardConfiguration("E034C6063295681A"));
    assert(&activeBoardConfiguration() == fx02);
    assert(!selectBoardConfiguration("unknown"));
    assert(&activeBoardConfiguration() == &safeBoardConfiguration());
    assert(activeBoardConfiguration().useDhcp);
    assert(activeBoardConfiguration().numPixels == 32);
    assert(activeBoardConfiguration().frameSize == 12);
    assert(activeBoardConfiguration().brightness == 16);
    assert(limitBrightness(activeBoardConfiguration(), 255) == 16);
    assert(limitBrightness(activeBoardConfiguration(), 8) == 8);
    assert(isValidBoardConfiguration(safeBoardConfiguration()));
    assert(!selectBoardConfiguration(nullptr));

    RuntimeBufferSizes sizes{};
    assert(runtimeBufferSizes(*fx02, sizes));
    assert(sizes.ledPixels == 170);
    assert(sizes.framePixels == 304);
    assert(sizes.shuffleIndexes == 170);
    assert(sizes.scratchWords == 170);
    assert(runtimeBufferSizes(safeBoardConfiguration(), sizes));
    assert(sizes.ledPixels == 32);
    assert(sizes.framePixels == 48);
    assert(sizes.shuffleIndexes == 32);
    assert(sizes.scratchWords == 32);

    BoardConfiguration nearLimit = *fx02;
    nearLimit.numPixels = 12;
    nearLimit.frameSize = 11;
    assert(isValidBoardConfiguration(nearLimit));
    assert(runtimeBufferSizes(nearLimit, sizes));
    assert(sizes.scratchWords == nearLimit.numPixels);
    assert((nearLimit.frameSize + 14) / 2 <= sizes.scratchWords);
    assert(nearLimit.frameSize * 2 <= sizes.scratchWords * sizeof(uint16_t));

    BoardConfiguration largest = *fx02;
    largest.numPixels = MAX_SUPPORTED_NUM_PIXELS;
    largest.frameSize = MAX_SUPPORTED_FRAME_SIZE;
    assert(isValidBoardConfiguration(largest));
    assert(runtimeBufferSizes(largest, sizes));
    assert(sizes.ledPixels == MAX_SUPPORTED_NUM_PIXELS);
    assert(sizes.scratchWords == MAX_SUPPORTED_NUM_PIXELS);

    {
        BoardBufferStorage<uint32_t> buffers;
        assert(!buffers.scratchWordView(0, 1));
        assert(!buffers.scratchByteView(0, 1));
        assert(buffers.allocate(*fx02, testAllocate, testFree));
        const uint32_t *const originalLeds = buffers.leds.get();
        const size_t originalAllocationCount = liveAllocations;
        assert(originalAllocationCount == 4);
        assert(buffers.sizes.ledPixels == 170);
        const auto allScratchBytes = buffers.scratchByteView(0, buffers.sizes.scratchWords * sizeof(uint16_t));
        assert(allScratchBytes);
        assert(allScratchBytes.size == buffers.sizes.scratchWords * sizeof(uint16_t));
        for (size_t i = 0; i < allScratchBytes.size; ++i)
            assert(allScratchBytes.data[i] == 0);

        const auto wordView = buffers.scratchWordView(2, 4);
        assert(wordView);
        assert(wordView.data == buffers.scratch.get() + 2);
        assert(wordView.size == 4);
        assert(!buffers.scratchWordView(buffers.sizes.scratchWords - 1, 2));
        assert(!buffers.scratchWordView(buffers.sizes.scratchWords + 1, 0));

        const auto wetTrail = buffers.scratchByteView(0, fx02->frameSize);
        const auto wetTrailColors = buffers.scratchByteView(fx02->frameSize, fx02->frameSize);
        assert(wetTrail && wetTrailColors);
        assert(wetTrail.size == fx02->frameSize);
        assert(wetTrailColors.data == wetTrail.data + wetTrail.size);
        assert(wetTrailColors.size == fx02->frameSize);
        assert(!buffers.scratchByteView(allScratchBytes.size - 1, 2));
        assert(!buffers.scratchByteView(allScratchBytes.size + 1, 0));

        buffers.scratch[0] = 0x5a3c;
        assert(wetTrail.data[0] == reinterpret_cast<uint8_t *>(&buffers.scratch[0])[0]);
        wetTrail.data[0] = 0x5a;
        assert(allScratchBytes.data[0] == 0x5a);
        wetTrailColors.data[0] = 0xa5;
        assert(allScratchBytes.data[fx02->frameSize] == 0xa5);

        for (size_t failOffset = 1; failOffset <= 4; ++failOffset) {
            failAllocationAt = allocationAttempts + failOffset;
            assert(!buffers.allocate(safeBoardConfiguration(), testAllocate, testFree));
            assert(buffers.leds.get() == originalLeds);
            assert(buffers.sizes.ledPixels == 170);
            assert(liveAllocations == originalAllocationCount);
        }
        failAllocationAt = 0;
        assert(buffers.allocate(safeBoardConfiguration(), testAllocate, testFree));
        assert(buffers.sizes.ledPixels == 32);
        assert(buffers.sizes.framePixels == 48);
        assert(buffers.sizes.scratchWords == 32);
        assert(liveAllocations == originalAllocationCount);

        assert(buffers.allocate(nearLimit, testAllocate, testFree));
        assert(buffers.sizes.scratchWords == nearLimit.numPixels);
        assert(buffers.scratchWordView(0, (nearLimit.frameSize + 14) / 2));
        assert(buffers.scratchByteView(0, nearLimit.frameSize * 2));
        assert(liveAllocations == originalAllocationCount);

        assert(buffers.allocate(largest, testAllocate, testFree));
        assert(buffers.sizes.ledPixels == MAX_SUPPORTED_NUM_PIXELS);
        assert(buffers.sizes.scratchWords == MAX_SUPPORTED_NUM_PIXELS);
        assert(liveAllocations == originalAllocationCount);
    }
    assert(liveAllocations == 0);

    BoardConfiguration invalid = *fx02;
    invalid.numPixels = MAX_SUPPORTED_NUM_PIXELS + 1;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *fx02;
    invalid.frameSize = MAX_SUPPORTED_FRAME_SIZE + 1;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *fx02;
    invalid.frameSize = 10;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *fx02;
    invalid.frameSize = invalid.numPixels;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *fx02;
    invalid.numPixels = 0;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *fx02;
    invalid.vccDivR4 = 0;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *fx02;
    invalid.ip[3] = 0;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *fx02;
    invalid.maxBrightness = 100;
    invalid.brightness = 101;
    assert(!isValidBoardConfiguration(invalid));
    return 0;
}