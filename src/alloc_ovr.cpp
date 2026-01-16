// MIT License
//
// Copyright (c) 2026 by Dan Luca. All rights reserved.
//
#include <cstdlib>
#include <cstring>
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

 /**
  *  @brief Warm-reboots the chip in normal mode
  *  See RP2040Support.h (cores/rp2040/RP2040Support.h)
  */
[[noreturn]] void reboot() {
    watchdog_reboot(0, 0, 10);
    while (true)
        tight_loop_contents();
}


extern "C" {

// malloc wrapper - mark as weak so it doesn't collide with the USB/Arduino internal wrapper
__weak void* __wrap_malloc(size_t size) {
    return operator new(size);
}

// free wrapper - mark as weak so it doesn't collide with the USB/Arduino internal wrapper
__weak void __wrap_free(void* ptr) {
    operator delete(ptr);
}

// calloc wrapper - mark as weak so it doesn't collide with the USB/Arduino internal wrapper
__weak void* __wrap_calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* p = pvPortMalloc(total);
    if (p) memset(p, 0, total);
    return p;
}

// realloc wrapper - mark as weak so it doesn't collide with the USB/Arduino internal wrapper
// void* __wrap_realloc(void* ptr, size_t size) {
//     if (size == 0) {
//         __wrap_free(ptr);
//         return nullptr;
//     }

//     // FreeRTOS Heap4 doesn't have a native realloc; we must implement it.
//     void* new_ptr = __wrap_malloc(size);
//     if (new_ptr && ptr) {
//         // Warning: This simple realloc assumes the old size (risky)
//         // In practice, Heap4 doesn't track sizes for external use.
//         // A safer way is to use Heap_5 or a custom tracker.
//         // If your code relies heavily on realloc, consider Heap_3 (libc wrapper).
//         memcpy(new_ptr, ptr, size);
//         __wrap_free(ptr);
//     }
//     return new_ptr;
// }


} // extern "C"

// Ensure the new and delete operators are re-directed to the thread-safe pvPortMalloc/pvPortFree counterparts
// Works in conjunction with Heap4 memory management strategy, providing thread-safe memory allocation for FreeRTOS tasks

void* operator new(size_t size) {
    void *ptr = pvPortMalloc(size);
    return ptr;
}

void* operator new[](size_t size) {
    void *ptr = pvPortMalloc(size);
    return ptr;
}

void operator delete(void* p) noexcept {
    vPortFree(p);
}

void operator delete[](void* p) noexcept {
    vPortFree(p);
}
