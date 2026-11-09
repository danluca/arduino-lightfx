---
type: Architecture
title: Memory model
description: The 192 KB heap_4 FreeRTOS heap next to the newlib and lwIP heaps, the board-sized LED buffers, and the coding patterns used to keep fragmentation and leaks down.
tags: [memory, heap, freertos, fragmentation, lwip]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/platformio.ini
    title: platformio.ini
  - id: efx
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/efx_setup.cpp
    title: src/efx_setup.cpp
  - id: sysinfo
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/src/sysinfo_runtime.cpp
    title: src/sysinfo_runtime.cpp (heapStats)
  - id: lwip
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/lwipopts.h
    title: include/lwipopts.h
  - id: claudeinst
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/.claude/instructions.md
    title: .claude/instructions.md
    author: human:danluca
---

# Heaps

The RP2350 has 520 KB of SRAM (the board file declares 512 KB). Three allocators share it:

* **FreeRTOS heap:** `configTOTAL_HEAP_SIZE = 192*1024`, up from the 164 KB default. `configFREERTOS_HEAP_SCHEME=4` (heap_4) is the only FreeRTOS scheme that merges adjacent free blocks. `new`/`delete` in application code go through `pvPortMalloc`.[^pio]
* **newlib heap (malloc/free):** some framework allocations still use it. The `platformio.ini` comment says the FreeRTOS heap is sized to leave room for it, so do not grow `configTOTAL_HEAP_SIZE` without checking the remaining RAM.[^pio]
* **lwIP heap:** the framework's precompiled `liblwip.a` sizes apply: `MEM_SIZE = 16384` plus static pools (`PBUF_POOL_SIZE 24`, `MEMP_NUM_TCP_PCB 5`, `MEMP_NUM_UDP_PCB 7`, `MEMP_NUM_TCP_SEG 32`). They come from the framework's `include/lwipopts.h` (the master) and cannot be changed from the project's `include/lwipopts.h`; see [framework customizations](/build/framework-customizations.md). `MEM_LIBC_MALLOC = 0`, so lwIP does not use malloc.[^lwip]

Other points:

* **Malloc failure** calls `vApplicationMallocFailedHook`, which writes reset marker `0xA11CE523` and reboots. See [watchdog and health](/architecture/watchdog-and-health.md).
* **Visibility:** `GET /tasks.json` → `heap` reports `totalHeap`, `freeHeap`, `usedHeap`, `minHeap` (the lowest free amount ever), `maxHeapBlock`, `minHeapBlock` and `freeBlocks` from `vPortGetHeapStats`, plus `stackPointer`, `freeStack`, `psramSize`, `psramHeapTotal/Free/Used` (0 on the Plasma 2350 W, which has no PSRAM) and `logMinBufferSpace`. The web stats page shows the same data.[^sysinfo]
* There are no malloc wrappers. Heap figures come only from FreeRTOS.

# Board-sized LED and effect buffers

The FX task allocates its LED storage once in `ledStripInit()` after the UID-selected board configuration is available. `BoardBufferStorage` owns the allocations with `std::unique_ptr` custom deleters; all arrays are committed together only after every allocation succeeds. The buffers use checked C `malloc`/`free`, so a null return is recoverable and does not invoke the FreeRTOS `pvPortMalloc` failure hook that reboots the board. FastLED views are published only after the complete allocation succeeds.[^efx]

| Symbol | Size | Purpose |
|---|---|---|
| `leds[NUM_PIXELS]` | 3 B × `NUM_PIXELS` (3 KB on the 1000 px Tree board) | The FastLED output array |
| `ledSet` | view | All of `leds` as a `CRGBSet` |
| `tpl` | view, `FRAME_SIZE` | The template, or first frame, of the strip |
| `others` | view, `FRAME_SIZE..NUM_PIXELS-1` | The rest of the strip. Many effects draw into `tpl` and then call `replicateSet(tpl, others)` |
| `frame` | 3 B × 4 × `frameSize` | Effect scratch buffer; four frames cover the largest current effect view |
| `stripShuffleIndex` | 2 B × `numPixels` | Random permutation used by spot effects and transitions |
| Effect scratch | 2 B × `numPixels` | Generic scratch region available to effects as checked, offset byte or 16-bit views; currently shared by FxF4’s easing curve and FxJ2’s two trail arrays |

Compile-time guards: `10 < FRAME_SIZE < NUM_PIXELS` and `PIXEL_BUFFER_SPACE > 3×FRAME_SIZE`. `MAX_NUM_PIXELS` is 1024, and the Tree board already uses 1000.

`LedBufferOwner` owns all board-sized storage and its FastLED views. It initializes those views once after successful allocation and rejects reinitialization, so FastLED never retains a pointer into moved or freed LED storage. Effect scratch reserves one aligned 16-bit word per strip pixel; effects request bounded word or byte views during setup. FxF4 uses a word view for its easing curve, while FxJ2 requests two adjacent byte views for trail intensity and color. The registry’s FX-task-only lifecycle deletes the active effect before constructing the next, which is the exclusivity guarantee for this shared region.

# Patterns used to limit fragmentation and leaks

1. **One effect at a time.** The registry deletes the old effect object before it creates the next one. See [effect registry](/effects/effect-registry.md). Effects that hold `std::vector` or other dynamic buffers (for example the FXI4 seed buffer) should free them in `cleanup()`.
2. **Large temporary strings go on the heap and get freed explicitly.** For example `new String()` plus `reserve(measureJson(doc))`, then `delete`, in `saveFxState` and `saveCalibrationInfo`.
3. **`JsonDocument::clear()`** is called before each document goes out of scope.
4. **Queues hold values, not pointers.** No allocation per message.
5. **Persistent objects are created once.** For example the timers. The `alarmCheck` timer is re-armed rather than recreated. The NTP client is the framework's lwIP SNTP client, so the firmware owns no NTP socket or buffer; on a Wi-Fi reconnect it is stopped and started again (`endNTP`, `timeBegin`).
6. **FX state saves are coalesced.** `fxStateDirty` collects all save requests and `fx_run` writes `/state.json` at most once per iteration.
7. **Avoid the regex library.** `UriRegex` would add about 300 KB of flash. Upload paths come from the `X-Path` header instead.[^claudeinst]

Project guidance: watch allocation, freeing and bounds. Prefer RAII and smart pointers over raw `new`/`delete` (AGENTS.md). Check the stats page after every change.[^claudeinst]

[^pio]: platformio.ini
[^efx]: src/efx_setup.cpp
[^sysinfo]: src/sysinfo_runtime.cpp (heapStats)
[^lwip]: include/lwipopts.h
[^claudeinst]: .claude/instructions.md
