---
type: Architecture
title: Memory model
description: The 144 KB heap_4 FreeRTOS heap, the static LED buffers, and the coding patterns used to keep fragmentation and leaks down.
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

# Static buffers (not on the heap)

Declared in the `fx` namespace in `efx_setup.cpp`. Only the FX task touches them.[^efx]

| Symbol | Size | Purpose |
|---|---|---|
| `leds[NUM_PIXELS]` | 3 B × `NUM_PIXELS` | The FastLED output array |
| `ledSet` | view | All of `leds` as a `CRGBSet` |
| `tpl` | view, `FRAME_SIZE` | The template, or first frame, of the strip |
| `others` | view, `FRAME_SIZE..NUM_PIXELS-1` | The rest of the strip. Many effects draw into `tpl` and then call `replicateSet(tpl, others)` |
| `frame` | `PIXEL_BUFFER_SPACE` (4×`FRAME_SIZE`) | Scratch buffer for effects |
| `stripShuffleIndex[NUM_PIXELS]` | 2 B × `NUM_PIXELS` | Random permutation used by spot effects and transitions |

Compile-time guards: `10 < FRAME_SIZE < NUM_PIXELS` and `PIXEL_BUFFER_SPACE > 3×FRAME_SIZE`. `MAX_NUM_PIXELS` is 1024.

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
