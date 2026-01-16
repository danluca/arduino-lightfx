// MIT License
//
// Copyright (c) 2026 by Dan Luca. All rights reserved.
//
#include "FreeRTOS.h"
#include "task.h"
#include "hardware/watchdog.h"
#include <SerialUSB.h>

 /**
  *  @brief Warm-reboots the chip in normal mode
  *  See RP2040Support.h (cores/rp2040/RP2040Support.h)
  */
[[noreturn]] void reboot() {
    if (Serial)
        Serial.println("PANIC AT THE DISCO: Rebooting due to memory allocation failure");
    watchdog_reboot(0, 0, 10);
    while (true)
        tight_loop_contents();
}


// Ensure the new and delete operators are re-directed to the thread-safe pvPortMalloc/pvPortFree counterparts
// Works in conjunction with Heap4 memory management strategy, providing thread-safe memory allocation for FreeRTOS tasks

void* operator new(size_t size) {
    void *ptr = pvPortMalloc(size);
    //warm reboot if allocation fails - it means the heap is too fragmented
    if (!ptr)
        reboot();
    return ptr;
}

void* operator new[](size_t size) {
    void *ptr = pvPortMalloc(size);
    //warm reboot if allocation fails - it means the heap is too fragmented
    if (!ptr)
        reboot();
    return ptr;
}

void* __wrap_malloc(size_t size) {
    return operator new(size);
}

void* __wrap_calloc(size_t nmemb, size_t size) {
    return operator new[](nmemb * size);
}

void __wrap_free(void* ptr) {
    operator delete(ptr);
}

void operator delete(void* p) noexcept {
    vPortFree(p);
}

void operator delete[](void* p) noexcept {
    vPortFree(p);
    }