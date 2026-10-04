# RP2350 LightFX

A sophisticated, feature-rich LED lighting effects controller for WS2811/WS2812 LED strips, built on the Pimoroni Plasma 2350 W. This project harnesses the dual-core power of the RP2350 microcontroller to deliver smooth, complex lighting effects with web-based configuration, time-aware scheduling, and holiday-themed color palettes.

## Features

- **50 Custom Light Effects**: Original animations and community-inspired patterns
- **Effect Registry**: Auto-registration system for modular effect management
- **Web Interface**: Real-time configuration via WiFi-enabled web server with JSON REST API
- **Smart Scheduling**: Time-aware dimming, sleep schedule and automatic holiday detection
- **Multi-Core Architecture**: FreeRTOS-based task distribution across both RP2350 cores
- **Multi-Board Sync**: A master board broadcasts effect changes to the other boards (static list and mDNS discovery)
- **OTA Updates**: Over-the-air firmware updates
- **Memory & Health Monitoring**: Real-time heap and task stack tracking from FreeRTOS statistics; watchdog with recorded reboot causes

## Target Hardware

**Board**: [Pimoroni Plasma 2350 W](https://shop.pimoroni.com/products/plasma-2350-w)
- Dual-core RP2350 @ 150MHz (ARM Cortex-M33)
- 520KB SRAM (192KB configured for the FreeRTOS heap)
- 4MB firmware + 1MB LittleFS filesystem
- WiFi (Infineon CYW43439, 2.4GHz)
- On-board RGB status LED, LED strip data output on GPIO 15
- PIO state machines for FastLED

The board definition is part of the project: `boards/pimoroni_plasma2350w.json`.

**LED Strips** (per board, see `include/config.h`):
- **Dev** (`Xmas2350`): Pimoroni 10m addressable RGB LED star wire, WS2812B, 66 pixels
- **Tree** (`FXPine`): WS2811 strings, 1000 pixels configured

The RP2040 version of this project (Arduino Nano RP2040 Connect, with a PDM microphone and IMU) lives on the `dev/12-fxe` and `rel/nanorp2040connect` branches.

## Quick Start

### Prerequisites

- **Python 3.12+** (Windows: Microsoft Store, Linux/Mac: package manager)
- **PlatformIO Core CLI** - [Installation guide](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html)
- **PowerShell 7** (`pwsh`) for the build and deploy scripts
- **IDE**: [CLion](https://www.jetbrains.com/clion/download) (commercial) or [VS Code](https://code.visualstudio.com/) (free, recommended for beginners)

### Build & Upload

```powershell
# Build the project (Dev board by default; -board Tree for the other one)
./build.ps1 -board Dev

# Upload firmware over USB
./update.ps1 -board Dev

# OTA upgrade (after initial flash)
./ota_upgrade.ps1 -board Tree

# Monitor serial output (needs a -log or -dbg build; release builds have no USB serial)
./seriallog.ps1
```

**PlatformIO CLI**:
```bash
# Build (BOARD_ID defaults to 2 - Tree - in config.h; set PLATFORMIO_BUILD_FLAGS=-DBOARD_ID=1 for Dev)
pio run -e rp2350-rel

# Upload (picotool)
pio run -e rp2350-rel -t upload

# Monitor
pio device monitor
```

## Configuration

1. **Secrets**: Copy `include/secrets.h.example` to `include/secrets.h` (git-ignored - never commit it) and fill in:
```cpp
#define WF_SSID "your-network"
#define WF_PSW  "your-password"
#define FW_AUTH_TOKEN "your-upload-token"   // X-Token for firmware (POST /fw) and file (POST/PUT /upload) uploads
```
   The build fails if `FW_AUTH_TOKEN` is missing. The upload scripts (`ota_upgrade.ps1`, `scripts/upload_audio_seed.*`) read the token from
   the `LIGHTFX_AUTH_TOKEN` environment variable when set, otherwise from `include/secrets.h`.

2. **LED Configuration**: Edit the board block in `include/config.h`:
   - Chipset and color order
   - Number of pixels and frame size
   - The data pin is `PIN_NEOPIXEL` (GPIO 15) on the Plasma 2350 W

3. **Network Settings**: Addresses come from DHCP. Reserve a fixed address per board MAC in the router, and keep `IP_ADDR` in
   `include/config.h` and `scripts/boards.ps1` in sync with it (the scripts use `scripts/boards.ps1`).

4. **Access Web Interface**: Navigate to the board's IP address, or `http://lightfx-<device>.local/` (mDNS), after connection

## Architecture

### Design Philosophy

This project prioritizes **performance and features over portability** - unapologetically optimized for the RP2350 platform. The architecture leverages the full capabilities of the dual-core processor:

**Effect System**:
- Object-oriented design with `LedEffect` base class
- Auto-registration pattern via `EffectRegistry`
- Each effect encapsulates its own state and rendering logic
- 50 effects organized across multiple source files (fxA.cpp through fxK.cpp)

**Time Intelligence**:
- Automatic holiday detection (day/month based)
- Dynamic color palette selection for occasions
- Time-of-day dimming curves and an optional sleep schedule
- NTP synchronization for accurate timekeeping

**Resource Management**:
- Heap and task stack monitoring from FreeRTOS statistics (`vPortGetHeapStats`, see `sysinfo_runtime.cpp`)
- Heap 4 (FreeRTOS) memory allocator for optimized memory footprint
- 192KB heap configuration (up from the 164KB default), leaving room for the newlib and lwIP heaps
- Real-time memory statistics via web interface (`/stats.html`)

### Core Technologies

**Framework**: [Arduino-Pico](https://github.com/earlephilhower/arduino-pico) by Earle F. Philhower, from the [danluca/arduino-pico](https://github.com/danluca/arduino-pico/tree/feat/stable) fork
- FreeRTOS-based multi-threading
- Efficient resource management
- Native dual-core support
- C++17 standard

**WiFi**: arduino-pico `WiFi` library on the on-board CYW43439, with lwIP (options tuned in `include/lwipopts.h`)
- DHCP addressing with router reservations
- `LEAmDNS` for mDNS advertising and board discovery
- All networking runs on Core 0

**LED Control**: [FastLED](https://github.com/FastLED/FastLED) 3.10.3 (pinned in `platformio.ini`)
- Hardware PIO acceleration on RP2350
- Non-blocking signal generation
- Rich color manipulation primitives

**Key Libraries**:
- ArduinoJson 7.0+ (configuration/REST API)
- StreamUtils
- Local libraries under `lib/`: RestWebServer, FilesystemTask, SchedulerExt, TimeLib, PicoLog, Utils

### Multi-Threading Architecture

**CORE 0** (WiFi & System):
- **CORE0 Task**: Web server, WiFi management and communications
- **ALM Task**: Time-based alarm processing
- **FS Task**: Serialized filesystem operations (LittleFS not thread-safe)

**CORE 1** (Effects & Diagnostics):
- **FX Task**: LED effect rendering loop, owns the hardware watchdog
- **CORE1 Task**: Diagnostics, temperature and voltage monitoring, task statistics

**Stack Sizes**: CORE0 and CORE1 task stack depths are 3072 words (from the default 1024), set through the `configCORE0_TASK_STACK_DEPTH` and
`configCORE1_TASK_STACK_DEPTH` build flags in `platformio.ini`; these are honored by the [arduino-pico fork](https://github.com/danluca/arduino-pico/tree/feat/stable) used by this project.

A detailed description of tasks, queues, timers, the REST API and known issues is in the knowledge bundle at [docs/okf](docs/okf/index.md).

## Hardware Integration

The Plasma 2350 W drives the LED strip directly from its LED connector (data on GPIO 15).

**Note**: `docs/Controller_Schematic.pdf`, `docs/Controller_PCB.pdf` and `docs/Controller_PCB.gerber.zip` describe the controller
built for the RP2040 version (Nano RP2040 Connect with an LM7805 regulator and a 74HCT125 level shifter); they do not apply to the Plasma 2350 W.
The supply voltage reading assumes a resistive divider on A0 (`VCC_DIV_R4`/`VCC_DIV_R5` in `config.h`).

## Contributing

Contributions welcome! This project benefits from:

**Bug Reports & Testing**:
- Different RP2350 board variants
- Alternative LED strip configurations
- Memory optimization findings

**New Effects**:
- Follow the `LedEffect` base class pattern
- Add to appropriate `fx*.cpp` file or create new category
- Register via `EffectRegistry`; append at the end so registry indexes (used by broadcast and saved state) do not shift
- Test memory impact (use web stats page)

**Code Standards**:
- C++17 features encouraged
- Verify standard: `pio run -v` (look for `-std=gnu++17`)
- Match existing code style
- Comment complex algorithms
- Consider memory constraints (192KB heap)

**Development Workflow**:
1. Fork and create feature branch from `dev/plasma2350w`
2. Test on hardware (memory, performance, visual quality)
3. Submit PR to `dev/plasma2350w` branch
4. Include description of effect/fix and any memory implications

## Inspiration & Credits

Built with [FastLED](https://github.com/FastLED/FastLED) at its core. Many effects inspired by the incredible FastLED community:

- [atuline/FastLED-Demos](https://github.com/atuline/FastLED-Demos) - Comprehensive effect examples
- [atuline/FastLED-SoundReactive](https://github.com/atuline/FastLED-SoundReactive) - Audio reactive patterns
- [davepl/DavesGarageLEDSeries](https://github.com/davepl/DavesGarageLEDSeries) - Educational LED programming
- [s-marley/FastLED-basics](https://github.com/s-marley/FastLED-basics) - Foundational concepts
- [chemdoc77](https://github.com/chemdoc77), [marmilicious](https://github.com/marmilicious), [marcmerlin](https://github.com/marcmerlin)
- [AaronLiddiment](https://github.com/AaronLiddiment), [evilgeniuslabs](https://github.com/evilgeniuslabs), [jasoncoon](https://github.com/jasoncoon)
- [brimshot/quickPatterns](https://github.com/brimshot/quickPatterns)

## Project Structure

```
rp2350-lightfx/
├── boards/           # PlatformIO board definition (Plasma 2350 W)
├── src/              # Source files
│   ├── Main.cpp      # Entry point
│   ├── fx*.cpp       # Effect implementations (A-K categories)
│   ├── web_server.cpp
│   ├── EffectRegistry.cpp
│   └── ...
├── include/          # Headers
│   ├── config.h      # Hardware configuration
│   ├── secrets.h     # WiFi credentials, upload token (not in repo - see secrets.h.example)
│   ├── LedEffect.h   # Effect base class
│   └── ...
├── lib/              # Local libraries
├── www/              # Web interface assets (run tools/include_www.ps1 after editing)
├── scripts/          # Build helpers, board map, audio seed tools
├── tools/            # Utility scripts
├── docs/             # Documentation, including the knowledge bundle (docs/okf)
├── build.ps1         # PowerShell build script
├── ota_upgrade.ps1   # OTA update script
└── platformio.ini    # PlatformIO configuration
```

## Troubleshooting

**WiFi Not Connecting**:
- Verify credentials in `secrets.h`
- Check 2.4GHz network compatibility
- Check the router's DHCP reservation for the board

**Memory Issues**:
- Monitor via web stats page
- Check the `Heap ::` lines in the serial log
- Reduce heap via `configTOTAL_HEAP_SIZE` if needed

**Effects Not Smooth**:
- Verify PIO support is enabled
- Check FX task isn't starved (Core 1)
- Reduce pixel count if necessary

**Unexpected Reboots**:
- Check `/health.json` for the last reboot reason, reset marker and FX stage
- Check `watchdogReboots` in `/status.json`

**OTA Update Fails**:
- Check the board address in `scripts/boards.ps1` matches the router reservation
- Check network connectivity
- Verify the script token (`LIGHTFX_AUTH_TOKEN` or `include/secrets.h`) matches the `FW_AUTH_TOKEN` the board firmware was built with
- Verify sufficient filesystem space (the image is staged in the 1MB LittleFS partition)

## Advanced: Hardware Debugging

Step-through debugging uses the `rp2350-dbg` environment with a Raspberry Pi Debug Probe (`picoprobe`). [docs/debugging.md](docs/debugging.md)
was written for the Nano RP2040 Connect; its custom OpenOCD flash patch and SWD pad soldering do not apply to the Plasma 2350 W.

**Quick summary**:
- Use an OpenOCD build with RP2350 support
- FastLED requires `FASTLED_ALLOW_INTERRUPTS=0` in debug builds (set by the debug environment)

## License

This project is released under the MIT License. See [LICENSE](LICENSE) file for details.

---

**Note**: This is a personal project optimized for a specific installation. While the code is shared openly, it's designed for the Pimoroni Plasma 2350 W and may require modifications for other boards or setups. Portability is not a primary goal - performance and features come first! 
