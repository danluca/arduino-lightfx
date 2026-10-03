---
type: Issue
title: Mic queue item size
description: micQueue is created with item size sizeof(AudioActionMessage*) (4 bytes), but PUT /fx sends an 8-byte AudioActionMessage by value. The data field is dropped and the audio threshold becomes 0.
tags: [bug, audio, queue, mic]
severity: medium
issue_state: fixed
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: taskmsg
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/task_msg.cpp
    title: src/task_msg.cpp (micQueue creation)
  - id: web
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/web_server.cpp
    title: src/web_server.cpp (handlePutConfig, audioThreshold)
  - id: mic
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/mic.cpp
    title: src/mic.cpp (mic_run)
---

# Defect

* `micQueue = xQueueCreate(10, sizeof(AudioActionMessage*))` gives 4-byte items on the RP2040. The comment says producers send pointers and the consumer deletes them.[^taskmsg]
* `handlePutConfig` sends `AudioActionMessage msg{AUDIO_THRESHOLD_UPDATE, value}` **by value**. The struct is `{uint8_t action; [3 bytes of padding]; uint32_t data;}`, which is 8 bytes. The queue copies only the first 4 bytes.[^web]
* `mic_run` receives into a zero-initialized `AudioActionMessage msg{}`. The `action` byte arrives, but `data` stays **0**.[^mic]

# Impact

`PUT /fx {"audioThreshold": N}` sets `audioBumpThreshold = 0` and clears the histogram. The HTTP response still echoes N. With a threshold of 0, nearly every audio batch counts as a bump, so the FX task moves to the next effect at every 30-second check, effectively turning on auto-advance. The next `saveFxState()` writes `audioThreshold: 0` to `/state.json`, so the problem **survives reboots**.

# Fix options

1. Create the queue with `sizeof(AudioActionMessage)` and keep sending by value. This matches every other queue in the project. Recommended.
2. Or keep the pointer design: `new AudioActionMessage{…}` in the producer and `delete` in the consumer.

Recovery on an affected board: build with the fix, then `PUT /fx {"audioThreshold":5000}`, or edit `/state.json`.

# Resolution

Fixed in `d5de0ba` with option 1: `micQueue` is created with `sizeof(AudioActionMessage)`. Boards that already persisted a 0 threshold still need the recovery step above.

# Verification status

Found by reading the source (struct layout and queue item size). Not reproduced on hardware. The fix compiles; not yet tested on a board.

[^taskmsg]: src/task_msg.cpp (micQueue creation)
[^web]: src/web_server.cpp (handlePutConfig, audioThreshold)
[^mic]: src/mic.cpp (mic_run)
