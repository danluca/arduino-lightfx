// MIT License
//
// Copyright (c) 2026 by Dan Luca. All rights reserved.
//
#include "FreeRTOS.h"
#include "task.h"

// Ensure the new and delete operators are re-directed to the thread-safe pvPortMalloc/pvPortFree counterparts
// Works in conjunction with Heap4 memory management strategy, providing thread-safe memory allocation for FreeRTOS tasks

void* operator new(size_t size) {
    return pvPortMalloc(size);
}

void* operator new[](size_t size) {
    return pvPortMalloc(size);
}

void operator delete(void* p) noexcept {
    vPortFree(p);
}

void operator delete[](void* p) noexcept {
    vPortFree(p);
}