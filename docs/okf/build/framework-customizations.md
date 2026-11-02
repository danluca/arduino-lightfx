---
type: Configuration
title: Framework customizations
description: Departures from a stock toolchain. The project uses an arduino-pico fork, its own Plasma 2350 W board file, enlarged CORE task stacks, heap_4, the framework's precompiled lwIP (the framework's lwipopts.h is the master), FastLED pinned to 3.10.3, and web assets compiled into headers.
tags: [build, framework, arduino-pico, fastled, toolchain, lwip]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T23:45:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: pio
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/platformio.ini
    title: platformio.ini
  - id: board
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/boards/pimoroni_plasma2350w.json
    title: boards/pimoroni_plasma2350w.json
  - id: lwip
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/include/lwipopts.h
    title: include/lwipopts.h
  - id: util
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/scripts/util.ps1
    title: scripts/util.ps1 (ensureHeap4Strategy, prepCoreStackSize)
  - id: www
    resource: https://github.com/danluca/arduino-lightfx/blob/6d11a87/tools/include_www.ps1
    title: tools/include_www.ps1
---

# arduino-pico fork

`platform_packages = framework-arduinopico@https://github.com/danluca/arduino-pico.git#feat/stable` (the project switched to the `feat/stable` branch in `e9c1ff3`). The platform is `maxgerhardt/platform-raspberrypi`.[^pio]

Why: the stock framework creates the CORE0 and CORE1 tasks with a fixed stack. This firmware sets 3072 words for each (`configCORE*_TASK_STACK_DEPTH`), which the fork honors. Earlier, `scripts/util.ps1` patched `freertos-main.cpp` (`prepCoreStackSize`) and copied `heap_4.c` into the framework (`ensureHeap4Strategy`). Those calls are now **commented out**, because the fork and the build flags provide both.[^util]

# Board file

`boards/pimoroni_plasma2350w.json` is a project-local PlatformIO board definition: RP2350 Cortex-M33 at 150 MHz, 512 KB RAM, 4 MB firmware maximum, `picotool` upload, the Plasma 2350 W variant, and the CYW43 defines. Debug settings point at `rp2350.cfg` and `rp2350.svd`.[^board]

# lwIP options

**The framework's `include/lwipopts.h` is the master** (`<platformio packages>/framework-arduinopico/include/lwipopts.h`). The lwIP stack is not built from this project: the framework links the precompiled `lib/rp2350/liblwip.a`, built from the framework's own `lwipopts.h`. That library holds the lwIP heap, the pbuf and PCB pools, the TCP settings and `apps/sntp/sntp.c`.

The firmware therefore runs on the framework's sizes, confirmed from the symbol sizes in the `rp2350-rel` ELF:

| Setting | Value in the firmware |
|---|---|
| `MEM_SIZE` (lwIP heap) | 16 KB |
| `PBUF_POOL_SIZE` | 24 |
| `MEMP_NUM_TCP_PCB` | 5 |
| `MEMP_NUM_UDP_PCB` | 7 |
| `MEMP_NUM_TCP_SEG` | 32 |
| `SNTP_MAX_SERVERS` | 2 |

These cannot be changed from the project. Changing them needs a rebuild of `liblwip.a` from the framework fork (`tools/libpico/make-libpico.sh`) with different options in the fork's `include/lwipopts.h`. Larger values cost RAM (40 pbufs alone is about 61 KB), and no buffer exhaustion has been seen so far.

`include/lwipopts.h` in this project is trimmed to what the project sources see through the lwIP headers (included by the framework `WiFi`, `LEAmDNS` and `HTTPClient` libraries): the platform hooks (critical section, assert, random) and the feature switches that shape lwIP structures (`NO_SYS`, `LWIP_DHCP`, `LWIP_DNS`, `LWIP_IGMP`, `LWIP_NETIF_*`, `LWIP_TCP_KEEPALIVE`, `TCP_LISTEN_BACKLOG`, `MDNS_MAX_SERVICES` and so on). Its values must stay identical to the framework file, so the project code and the library agree on the structure layouts; when the framework file changes (framework or fork update), mirror the change. A macro dump of `lwip/opt.h` with each file first on the include path showed no differences in these options.[^lwip]

**NTP settings:** the SNTP client is part of the precompiled lwIP, so SNTP macros in the project's `lwipopts.h` have no effect on it. The two NTP settings that matter are build flags in `platformio.ini` instead: `-D SNTP_UPDATE_DELAY=43200000UL` (12 h poll) and `-Wl,--wrap=__setSystemTime` (routes the SNTP clock update through `__wrap___setSystemTime` in `lib/TimeLib`). See [time sync](/scheduling/time-sync.md).

# FastLED pinned

`fastled/FastLED @ 3.10.3` is pinned until the PlatformIO registry and GitHub versions of FastLED line up again; later versions introduce breaking changes (`12c243d`, `4f616bc`). Revisit this before upgrading.[^pio]

# Other library pins

`ArduinoJson ^7.0.0` (v7 API: `JsonDocument`, `to<JsonArray>()`) and `StreamUtils ^1.9.1`. Wi-Fi, `HTTPClient`, `LEAmDNS`, `PicoOTA` and LittleFS come from the framework. The RP2040 line's extra dependencies (LSM6DSOX, ECCX08, ArduinoHttpClient) are not used here. Local libraries are covered in [local libraries](/build/local-libraries.md).

# Web assets are compiled in

The UI under `www/` (`index.html`, `pixel.css/js`, `stats.html/css/js`) is served from flash through generated headers (`include/index_html.h`, `pixel_js.h`, …). **After editing anything in `www/`, run `tools/include_www.ps1`** to regenerate the headers, then rebuild. Each file becomes `inline constexpr auto <name> PROGMEM = R"~~~( … )~~~";`. Editing `www/` alone changes nothing on the board.[^www]

The pages load **jQuery 3.7.1 and CanvasJS from public CDNs** (`code.jquery.com`, `cdn.canvasjs.com`), so the browser needs Internet access for the UI to work, even though the board serves the pages itself. `include/jquery_min_js.h` exists, but the static-resource map does not serve it.

[^pio]: platformio.ini
[^board]: boards/pimoroni_plasma2350w.json
[^lwip]: include/lwipopts.h
[^util]: scripts/util.ps1 (ensureHeap4Strategy, prepCoreStackSize)
[^www]: tools/include_www.ps1
