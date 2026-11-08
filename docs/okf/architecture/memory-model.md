---
type: Architecture
title: Memory model
description: The 144 KB heap_4 FreeRTOS heap, board-sized LED buffers, and the coding patterns used to keep fragmentation and leaks down.
tags: [memory, heap, freertos, fragmentation]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/platformio.ini
    title: platformio.ini
  - id: efx
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/efx_setup.cpp
    title: src/efx_setup.cpp
  - id: sysinfo
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/sysinfo.cpp
    title: src/sysinfo.cpp (heapStats)
  - id: claudeinst
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/.claude/instructions.md
    title: .claude/instructions.md
    author: human:danluca
---

# Heap

* **Size:** `configTOTAL_HEAP_SIZE = 144*1024`, down from the 164 KB default, to keep RP2040 RAM use around 80%.[^pio]
* **Scheme:** `configFREERTOS_HEAP_SCHEME=4` (heap_4). It is the only FreeRTOS scheme that merges adjacent free blocks. `new`/`delete` go through `pvPortMalloc`.[^pio]
* **Malloc failure** calls `vApplicationMallocFailedHook`, which writes reset marker `0xA11CE523` and reboots. See [watchdog and health](/architecture/watchdog-and-health.md).
* **Visibility:** `GET /tasks.json` → `heap` reports `totalHeap`, `freeHeap`, `usedHeap`, `minHeap` (the lowest free amount ever), `maxHeapBlock`, `minHeapBlock` and `freeBlocks` from `vPortGetHeapStats`. The web stats page shows the same data.[^sysinfo]

Older docs said the heap was 128 KB and that `-Wl,--wrap,malloc` wrappers existed in an `alloc_ovr.cpp`; both were corrected. There are no malloc wrappers. See [documentation drift](/issues/documentation-drift.md).

# Board-sized LED and effect buffers

The FX task allocates its LED storage once in `ledStripInit()` after the UID-selected board configuration is available. `BoardBufferStorage` owns the allocations with `std::unique_ptr` custom deleters; all arrays are committed together only after every allocation succeeds. The buffers use checked C `malloc`/`free`, so a null return is recoverable and does not invoke the FreeRTOS `pvPortMalloc` failure hook that reboots the board. FastLED views are published only after the complete allocation succeeds.[^efx]

| Symbol | Size | Purpose |
|---|---|---|
| `leds` | 3 B × `numPixels` | FastLED output buffer for the active strip |
| `ledSet` | view | All of `leds` as a `CRGBSet` |
| `tpl` | view, `FRAME_SIZE` | The template, or first frame, of the strip |
| `others` | view, `FRAME_SIZE..NUM_PIXELS-1` | The rest of the strip. Many effects draw into `tpl` and then call `replicateSet(tpl, others)` |
| `frame` | 3 B × 4 × `frameSize` | Effect scratch buffer; four frames cover the largest current effect view |
| `stripShuffleIndex` | 2 B × `numPixels` | Random permutation used by spot effects and transitions |
| Effect scratch | 2 B × `numPixels` | Generic scratch region available to effects as checked, offset byte or 16-bit views; currently shared by FxF4’s easing curve and FxJ2’s two trail arrays |

Configuration validation enforces `10 < frameSize < numPixels`, `numPixels <= MAX_SUPPORTED_NUM_PIXELS` (320), and `frameSize <= MAX_SUPPORTED_FRAME_SIZE` (76). Those are operational limits for effects and LED timing, not preallocated capacities. A failed allocation frees any partial candidate buffers, leaves the current storage unpublished, disables LED effects, and allows CORE0 networking and OTA to continue. Fx task idle iterations keep reporting health so watchdog monitoring does not turn the degraded state into a reboot loop.

`LedBufferOwner` owns all board-sized storage and its FastLED views. It initializes those views once after successful allocation and rejects reinitialization, so FastLED never retains a pointer into moved or freed LED storage. Effect scratch reserves one aligned 16-bit word per strip pixel; effects request bounded word or byte views during setup. FxF4 uses a word view for its easing curve, while FxJ2 requests two adjacent byte views for trail intensity and color. The registry’s FX-task-only lifecycle deletes the active effect before constructing the next, which is the exclusivity guarantee for this shared region.

# Patterns used to limit fragmentation and leaks

1. **One effect at a time.** The registry deletes the old effect object before it creates the next one. See [effect registry](/effects/effect-registry.md). Effects that hold `std::vector` or other dynamic buffers (for example the FXI4 seed buffer) should free them in `cleanup()`.
2. **Large temporary strings go on the heap and get freed explicitly.** For example `new String()` plus `reserve(measureJson(doc))`, then `delete`, in `saveFxState`, `readSysInfo` and `saveSysInfo`.
3. **`JsonDocument::clear()`** is called before each document goes out of scope. A fix in `5b7c0fa` (2026-01-20) depended on this.
4. **Queues hold values, not pointers.** No allocation per message, except the intended pointer design for `micQueue` (see [mic queue item size](/issues/mic-queue-item-size.md)).
5. **Persistent objects are created once.** For example `ntpUDP`, `mdns`, `mUdp` and timers. They are only recreated on Wi-Fi reconnect, and the old instances are deleted first.
6. **Avoid the regex library.** `UriRegex` would add about 300 KB of flash. Upload paths come from the `X-Path` header instead.[^claudeinst]

Project guidance: watch allocation, freeing and bounds. Prefer RAII and smart pointers over raw `new`/`delete` (AGENTS.md). Check the stats page after every change.[^claudeinst]

[^pio]: platformio.ini
[^efx]: src/efx_setup.cpp
[^sysinfo]: src/sysinfo.cpp (heapStats)
[^claudeinst]: .claude/instructions.md
