# RP2040 LightFX

A sophisticated, feature-rich LED lighting effects controller for WS2811/WS2812 LED strips, built on the Arduino Nano RP2040 Connect. This project harnesses the dual-core power of the RP2040 microcontroller to deliver smooth, complex lighting effects with web-based configuration, time-aware scheduling, and holiday-themed color palettes.

## Features

- **70+ Custom Light Effects**: Original animations and community-inspired patterns
- **Effect Registry**: Auto-registration system for modular effect management
- **Web Interface**: Real-time configuration via WiFi-enabled web server with JSON REST API
- **Smart Scheduling**: Time-aware dimming and automatic holiday detection
- **Multi-Core Architecture**: FreeRTOS-based task distribution across both RP2040 cores
- **Audio Reactive**: PDM microphone integration for sound-responsive effects
- **OTA Updates**: Over-the-air firmware updates
- **Memory Monitoring**: Custom malloc wrappers with real-time heap tracking

## Target Hardware

**Primary Board**: [Arduino Nano RP2040 Connect](https://docs.arduino.cc/hardware/nano-rp2040-connect)
- Dual-core RP2040 @ 133MHz (ARM Cortex-M0+)
- 264KB SRAM (128KB configured for heap)
- 16MB Flash (4MB configured)
- WiFi (U-blox NINA W102)
- Built-in PDM microphone
- PIO state machines for FastLED

**Alternative**: [Pimoroni Plasma Stick 2040 W](https://shop.pimoroni.com/products/plasma-stick-2040-w) (5V version, minimal modifications needed)

**LED Strips**: WS2811 12V (IP65 rated, 30 LEDs/m) - tested with BTF-Lighting WS28115M30LW65

## Quick Start

### Prerequisites

- **Python 3.12+** (Windows: Microsoft Store, Linux/Mac: package manager)
- **PlatformIO Core CLI** - [Installation guide](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html)
- **IDE**: [CLion](https://www.jetbrains.com/clion/download) (commercial) or [VS Code](https://code.visualstudio.com/) (free, recommended for beginners)

### Build & Upload

```powershell
# Build the project
./build.ps1

# Upload firmware
./update.ps1

# OTA upgrade (after initial flash)
./ota_upgrade.ps1

# Monitor serial output
./seriallog.ps1
```

**PlatformIO CLI**:
```bash
# Build
pio run -e rp2040-rel

# Upload
pio run -e rp2040-rel -t upload

# Monitor
pio device monitor
```

## Configuration

1. **WiFi Setup**: Configure credentials in `include/secrets.h`:
```cpp
#define WIFI_SSID "your-network"
#define WIFI_PASSWORD "your-password"
```

2. **LED Configuration**: Edit `include/config.h`:
   - Pin assignment (default: GPIO 25 / D2)
   - Number of pixels
   - LED strip type

3. **Network Settings**: Static IP recommended for OTA updates (configured in `net_setup.cpp`)

4. **Access Web Interface**: Navigate to board's IP address after connection 

## Architecture

### Design Philosophy

This project prioritizes **performance and features over portability** - unapologetically optimized for the RP2040 platform. The architecture leverages the full capabilities of the dual-core processor:

**Effect System**:
- Object-oriented design with `LedEffect` base class
- Auto-registration pattern via `EffectRegistry`
- Each effect encapsulates its own state and rendering logic
- 70+ effects organized across multiple source files (fxA.cpp through fxK.cpp)

**Time Intelligence**:
- Automatic holiday detection (day/month based)
- Dynamic color palette selection for occasions
- Time-of-day dimming curves
- NTP synchronization for accurate timekeeping

**Resource Management**:
- Custom memory allocation wrappers (`alloc_ovr.cpp`)
- Heap monitoring and leak detection
- Heap 4 (FreeRTOS) memory allocator for optimized memory footprint
- 128KB heap configuration (optimized from 164KB default)
- Real-time memory statistics via web interface

### Core Technologies

**Framework**: [Arduino-Pico](https://github.com/earlephilhower/arduino-pico) by Earle F. Philhower
- FreeRTOS-based multi-threading
- Efficient resource management
- Native dual-core support
- C++17 standard

**WiFi**: [Arduino WiFiNINA](https://github.com/arduino-libraries/WiFiNINA) (optimized for U-blox NINA W102)
- Lower memory footprint than lwIP alternatives
- Static IP configuration recommended
- Core0 affinity for stability

**LED Control**: [FastLED](https://github.com/FastLED/FastLED) 3.9+
- Hardware PIO acceleration on RP2040
- Non-blocking PWM signal generation
- Rich color manipulation primitives

**Key Libraries**:
- ArduinoJson 7.0+ (configuration/REST API)
- Arduino_LSM6DSOX (accelerometer support)
- PDM (microphone audio processing) 

### Multi-Threading Architecture

**CORE 0** (WiFi & System):
- **CORE0 Task**: Web server and WiFi management
- **ALM Task**: Time-based alarm processing
- **FS Task**: Serialized filesystem operations (LittleFS not thread-safe)

**CORE 1** (Effects & Sensors):
- **FX Task**: LED effect rendering loop
- **Mic Task**: PDM microphone signal processing
- **CORE1 Task**: Diagnostics, logging, temperature monitoring

**Stack Optimization**: For optimal performance, modify `~/.platformio/packages/framework-arduinopico/libraries/FreeRTOS/src/variantHooks.cpp`:
- CORE0: 3072 bytes (from default 1024)
- CORE1: 1536 bytes (from default 1024)

## Hardware Integration

### Level Shifter Circuit

The Nano RP2040 Connect (3.3V logic) requires a level shifter to drive 12V WS2811 LED strips:

**Components**:
- **LM7805**: 12V → 5V regulator for board power
- **74HCT125**: 3.3V → 5V level shifter for data signal
- **Single 3-pin connector**: Elegant integration (12V power in, 5V PWM data out)

**Schematic**: [EasyEDA Project](https://pro.easyeda.com/editor#id=9c50130b250b4c23b522b4ac978d99bf)

### LED Strip Details

- **Model**: BTF-Lighting WS28115M30LW65 (Amazon)
- **Specifications**: 12V, 30 LEDs/m, IP65 rated
- **Note**: Each WS2811 "pixel" = 3 individual LEDs
- **Typical Installation**: ~300 pixels (longest path in house setup)
- **Control Pin**: GPIO 25 (D2 on Arduino Nano RP2040 Connect)

## Contributing

Contributions welcome! This project benefits from:

**Bug Reports & Testing**:
- Different RP2040 board variants
- Alternative LED strip configurations
- Memory optimization findings

**New Effects**:
- Follow the `LedEffect` base class pattern
- Add to appropriate `fx*.cpp` file or create new category
- Register via `EffectRegistry`
- Test memory impact (use web stats page)

**Code Standards**:
- C++17 features encouraged
- Verify standard: `pio run -v` (look for `-std=gnu++17`)
- Match existing code style
- Comment complex algorithms
- Consider memory constraints (128KB heap)

**Development Workflow**:
1. Fork and create feature branch from `dev/12-fxe`
2. Test on hardware (memory, performance, visual quality)
3. Submit PR to `dev/12-fxe` branch
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
rp2040-lightfx/
├── src/              # Source files
│   ├── Main.cpp      # Entry point
│   ├── fx*.cpp       # Effect implementations (A-K categories)
│   ├── web_server.cpp
│   ├── EffectRegistry.cpp
│   └── ...
├── include/          # Headers
│   ├── config.h      # Hardware configuration
│   ├── secrets.h     # WiFi credentials (not in repo)
│   ├── LedEffect.h   # Effect base class
│   └── ...
├── www/              # Web interface assets
├── tools/            # Utility scripts
├── docs/             # Documentation
├── build.ps1         # PowerShell build script
├── ota_upgrade.ps1   # OTA update script
└── platformio.ini    # PlatformIO configuration
```

## Troubleshooting

**WiFi Not Connecting**:
- Verify credentials in `secrets.h`
- Check 2.4GHz network compatibility
- Use static IP if DHCP fails

**Memory Issues**:
- Monitor via web stats page
- Check custom malloc wrappers are active
- Reduce heap via `configTOTAL_HEAP_SIZE` if needed

**Effects Not Smooth**:
- Verify PIO support is enabled
- Check FX task isn't starved (Core 1)
- Reduce pixel count if necessary

**OTA Update Fails**:
- Ensure static IP configured
- Check network connectivity
- Verify sufficient flash space

## Advanced: Hardware Debugging

For step-through debugging with breakpoints, see detailed setup in [docs/debugging.md](docs/debugging.md) (requires Raspberry Pi Debug Probe and custom OpenOCD build).

**Quick summary** - Known issues being worked around:
- Atmel AT25SF128A flash chip requires custom OpenOCD build
- SWD pad soldering required (fragile!)
- FastLED requires `FASTLED_ALLOW_INTERRUPTS=0` in debug builds
- SVD file parser limitations (peripheral view unavailable)

## License

This project is released under the MIT License. See [LICENSE](LICENSE) file for details.

---

**Note**: This is a personal project optimized for a specific house installation. While the code is shared openly, it's designed for the Nano RP2040 Connect and may require modifications for other boards or setups. Portability is not a primary goal - performance and features come first! 

