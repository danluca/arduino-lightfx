#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
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
    assert(count == 2);
    for (size_t i = 0; i < count; ++i)
        assert(isValidBoardConfiguration(configurations[i]));

    const BoardConfiguration *dev = findBoardConfiguration("SET_DEV_UID");
    const BoardConfiguration *tree = findBoardConfiguration("SET_TREE_UID");
    assert(dev != nullptr);
    assert(tree != nullptr);
    assert(findBoardConfiguration("set_dev_uid") == dev);
    assert(dev->numPixels == 66);
    assert(dev->frameSize == 22);
    assert(dev->ledChipset == LedChipset::WS2812B);
    assert(dev->colorOrder == LedColorOrder::BGR);
    assert(dev->useDhcp());
    assert(tree->numPixels == 1000);
    assert(tree->frameSize == 75);
    assert(tree->ledChipset == LedChipset::WS2811);
    assert(tree->colorOrder == LedColorOrder::RGB);
    assert(tree->useDhcp());
    assert(limitBrightness(*dev, 200) == 200);
    assert(limitBrightness(*dev, 255) == 255);
    assert(findBoardConfiguration(nullptr) == nullptr);
    assert(findBoardConfiguration("unknown") == nullptr);

    assert(selectBoardConfiguration("SET_DEV_UID"));
    assert(&activeBoardConfiguration() == dev);
    assert(selectBoardConfiguration("SET_TREE_UID"));
    assert(&activeBoardConfiguration() == tree);
    assert(!selectBoardConfiguration("UID0123456789"));
    assert(&activeBoardConfiguration() != &safeBoardConfiguration());
    assert(activeBoardConfiguration().useDhcp());
    assert(std::strcmp(activeBoardConfiguration().deviceName, "New-456789") == 0);
    assert(activeBoardConfiguration().numPixels == 32);
    assert(activeBoardConfiguration().frameSize == 14);
    assert(activeBoardConfiguration().brightness == 16);
    assert(limitBrightness(activeBoardConfiguration(), 255) == 16);
    assert(limitBrightness(activeBoardConfiguration(), 8) == 8);
    assert(isValidBoardConfiguration(safeBoardConfiguration()));
    assert(!selectBoardConfiguration(nullptr));
    assert(std::strcmp(activeBoardConfiguration().deviceName, "New-000000") == 0);

    RuntimeBufferSizes sizes{};
    assert(runtimeBufferSizes(*dev, sizes));
    assert(sizes.ledPixels == 66);
    assert(sizes.framePixels == 88);
    assert(sizes.shuffleIndexes == 66);
    assert(sizes.scratchWords == 66);
    assert(runtimeBufferSizes(*tree, sizes));
    assert(sizes.ledPixels == 1000);
    assert(sizes.framePixels == 300);
    assert(runtimeBufferSizes(safeBoardConfiguration(), sizes));
    assert(sizes.ledPixels == 32);
    assert(sizes.framePixels == 56);
    assert(sizes.shuffleIndexes == 32);
    assert(sizes.scratchWords == 32);

    BoardConfiguration nearLimit = *dev;
    nearLimit.numPixels = 28;
    nearLimit.frameSize = 14;
    assert(isValidBoardConfiguration(nearLimit));
    assert(runtimeBufferSizes(nearLimit, sizes));
    assert(sizes.scratchWords == nearLimit.numPixels);
    assert((nearLimit.frameSize + 14) / 2 <= sizes.scratchWords);
    assert(nearLimit.frameSize * 2 <= sizes.scratchWords * sizeof(uint16_t));

    BoardConfiguration largest = *dev;
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
        assert(buffers.allocate(*dev, testAllocate, testFree));
        const uint32_t *const originalLeds = buffers.leds.get();
        const size_t originalAllocationCount = liveAllocations;
        assert(originalAllocationCount == 4);
        assert(buffers.sizes.ledPixels == 66);
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

        const auto wetTrail = buffers.scratchByteView(0, dev->frameSize);
        const auto wetTrailColors = buffers.scratchByteView(dev->frameSize, dev->frameSize);
        assert(wetTrail && wetTrailColors);
        assert(wetTrail.size == dev->frameSize);
        assert(wetTrailColors.data == wetTrail.data + wetTrail.size);
        assert(wetTrailColors.size == dev->frameSize);
        assert(!buffers.scratchByteView(allScratchBytes.size - 1, 2));
        assert(!buffers.scratchByteView(allScratchBytes.size + 1, 0));

        buffers.scratch[0] = 0x5a3c;
        assert(wetTrail.data[0] == reinterpret_cast<uint8_t *>(&buffers.scratch[0])[0]);
        wetTrail.data[0] = 0x5a;
        assert(allScratchBytes.data[0] == 0x5a);
        wetTrailColors.data[0] = 0xa5;
        assert(allScratchBytes.data[dev->frameSize] == 0xa5);

        for (size_t failOffset = 1; failOffset <= 4; ++failOffset) {
            failAllocationAt = allocationAttempts + failOffset;
            assert(!buffers.allocate(safeBoardConfiguration(), testAllocate, testFree));
            assert(buffers.leds.get() == originalLeds);
            assert(buffers.sizes.ledPixels == 66);
            assert(liveAllocations == originalAllocationCount);
        }
        failAllocationAt = 0;
        assert(buffers.allocate(safeBoardConfiguration(), testAllocate, testFree));
        assert(buffers.sizes.ledPixels == 32);
        assert(buffers.sizes.framePixels == 56);
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

    BoardConfiguration invalid = *dev;
    invalid.numPixels = MAX_SUPPORTED_NUM_PIXELS + 1;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *dev;
    invalid.frameSize = MAX_SUPPORTED_FRAME_SIZE + 1;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *dev;
    invalid.frameSize = 13;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *dev;
    invalid.numPixels = 27;
    invalid.frameSize = 14;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *dev;
    invalid.numPixels = 0;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *dev;
    invalid.vccDivR4 = 0;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *dev;
    invalid.maxBrightness = 100;
    invalid.brightness = 101;
    assert(!isValidBoardConfiguration(invalid));
    invalid = *dev;
    invalid.ledChipset = static_cast<LedChipset>(255);
    assert(!isValidBoardConfiguration(invalid));
    invalid = *dev;
    invalid.colorOrder = static_cast<LedColorOrder>(255);
    assert(!isValidBoardConfiguration(invalid));
    return 0;
}