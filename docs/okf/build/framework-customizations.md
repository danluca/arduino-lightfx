---
type: Configuration
title: Framework customizations
description: Departures from a stock toolchain. The project uses an arduino-pico fork, enlarged CORE task stacks, heap_4, FastLED pinned to 3.10.3, and web assets compiled into headers.
tags: [build, framework, arduino-pico, fastled, toolchain]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/platformio.ini
    title: platformio.ini
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/scripts/util.ps1
    title: scripts/util.ps1 (ensureHeap4Strategy, prepCoreStackSize)
  - id: www
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/tools/include_www.ps1
    title: tools/include_www.ps1
---

# arduino-pico fork

`platform_packages = framework-arduinopico@https://github.com/danluca/arduino-pico.git#feat/stable`. The project switched to the owner's own fork in `d588e7b` (2026-05-24) and to the `feat/stable` branch in `d57284b` (2026-09-06). The platform is `maxgerhardt/platform-raspberrypi`.[^pio]

Why: the stock framework creates the CORE0 and CORE1 tasks with a fixed 1024-byte stack. This firmware needs 2048 bytes (`configCORE*_TASK_STACK_DEPTH`). Earlier, `scripts/util.ps1` patched `freertos-main.cpp` (`prepCoreStackSize`) and copied `heap_4.c` into the framework (`ensureHeap4Strategy`). Those calls are now **commented out**, because the fork and the build flags provide both.[^util]

The README still describes hand-editing `variantHooks.cpp` (3072 and 1536 bytes). That advice is outdated. See [documentation drift](/issues/documentation-drift.md).

# FastLED pinned

`fastled/FastLED @ 3.10.3` is pinned "until the mixup between platformio and fastled are fixed". At the time, GitHub's latest was 3.10.4 while the PlatformIO registry showed 3.10.5. Revisit this before upgrading.[^pio]

# Other library pins

`Arduino_LSM6DSOX ^1.1.2`, `ArduinoJson ^7.0.0` (v7 API: `JsonDocument`, `to<JsonArray>()`), `StreamUtils ^1.9.1`, `ArduinoECCX08 ^1.3.7`, `ArduinoHttpClient ^0.6.1`. Local libraries are covered in [local libraries](/build/local-libraries.md).

# Web assets are compiled in

The UI under `www/` (`index.html`, `pixel.css/js`, `stats.html/css/js`) is served from flash through generated headers (`include/index_html.h`, `pixel_js.h`, …). **After editing anything in `www/`, run `tools/include_www.ps1`** to regenerate the headers, then rebuild. Each file becomes `inline constexpr auto <name> PROGMEM = R"~~~( … )~~~";`. Editing `www/` alone changes nothing on the board.[^www]

The pages load **jQuery 3.7.1 and CanvasJS from public CDNs** (`code.jquery.com`, `cdn.canvasjs.com`), so the browser needs Internet access for the UI to work, even though the board serves the pages itself. `include/jquery_min_js.h` exists, but the static-resource map does not serve it, and the local `<script src="jquery.min.js">` line in `index.html` is commented out.

[^pio]: platformio.ini
[^util]: scripts/util.ps1 (ensureHeap4Strategy, prepCoreStackSize)
[^www]: tools/include_www.ps1
