# AGENTS.md

## Guidance

* Pay close attention to memory management – allocation, freeing, boundaries
* Pay close attention to thread safety
* Pay close attention to tasks management, work allocated to tasks, race conditions
* Prefer smart pointers, RAII over raw pointers and new/delete
* The knowledge bundle under `docs/okf/` (start at `docs/okf/index.md`) describes the architecture, tasks, queues, timers, REST API and known issues in detail

## Environment

* Build: PlatformIO
* IDE: CLion, VSCode with PlatformIO extension
* OS: Windows for code editing, compiling only. Linux Ubuntu for compiling and updating boards
  * The board is connected to the Linux Ubuntu host via USB 
  * The Ubuntu host can also monitor and capture serial output
* Board: Pimoroni Plasma 2350 W – RP2350 CPU (dual Cortex-M33 @ 150MHz), 520KB SRAM (192KB FreeRTOS heap), 4MB firmware + 1MB LittleFS.
  * Board definition is project-local: `boards/pimoroni_plasma2350w.json`
  * PlatformIO environments: `rp2350-rel` (release), `rp2350-dbg` (debug, Raspberry Pi Debug Probe)
  * LED data on `PIN_NEOPIXEL` (GPIO 15); on-board RGB status LED on GPIO 16/17/18
  * No microphone, IMU or secure element; diagnostics use the RP2350 internal temperature sensor, the A0 voltage divider and the hardware RNG
* Compiler: GCC ARM Embedded, C++17 standard
* Framework: arduino-pico, fork https://github.com/danluca/arduino-pico (branch `feat/stable`) of https://github.com/earlephilhower/arduino-pico, locally at ~/.platformio/packages/framework-arduinopico
  * FreeRTOS kernel (SMP, heap_4)
* Libraries: FastLED (pinned 3.10.3), ArduinoJson, StreamUtils, framework libraries (WiFi, HTTPClient, LEAmDNS, PicoOTA, LittleFS), local libraries under `lib/` (REST web server, filesystem task, scheduler, time, logging, utils)
* WiFi: Infineon CYW43439 (802.11b/g/n, 2.4GHz) on-board, through the arduino-pico `WiFi` library and lwIP (options in `include/lwipopts.h`)
  * IP addresses are assigned by DHCP; the router reserves a fixed address per board MAC (Dev 192.168.0.75, Tree 192.168.0.182 – see `scripts/boards.ps1`)
* Boards: `Dev` (BOARD_ID 1, "Xmas2350", 66 px WS2812B) and `Tree` (BOARD_ID 2, "FXPine", 1000 px WS2811) – see `include/config.h`
* Secrets: `include/secrets.h` is git-ignored (template `include/secrets.h.example`) – WiFi credentials and the upload token `FW_AUTH_TOKEN`; never commit secrets
* Branch: `dev/plasma2350w`. The RP2040 (Nano RP2040 Connect) code line lives on `dev/12-fxe` / `rel/nanorp2040connect` and shares the effects, scheduling and `lib/` code – fixes there often need porting both ways

## Project Summary
A sophisticated, feature-rich LED lighting effects controller for WS2811/WS2812 LED strips, built 
for the Pimoroni Plasma 2350 W board with WiFi and leveraging the awesome FastLED library. 
The project uses FreeRTOS to manage tasks. 
The dual-core RP2350 microcontroller allows for
delivering smooth, complex lighting effects with web-based configuration, time-aware scheduling, 
and holiday-themed color palettes.

### Features

- **50 Custom Light Effects**: Original animations and community-inspired patterns
- **Effect Registry**: Auto-registration system for modular effect management
- **Web Interface**: Real-time configuration via WiFi-enabled web server with JSON REST API
- **Smart Scheduling**: Time-aware dimming and automatic holiday detection
- **Multi-Core Architecture**: FreeRTOS-based task distribution across both RP2350 cores
- **Multi-Board Sync**: A master board broadcasts effect changes to other boards (static list and mDNS discovery)
- **OTA Updates**: Over-the-air firmware updates
- **Memory Monitoring**: Real-time heap and task stack tracking from FreeRTOS statistics
- **Health Monitoring**: Watchdog owned by the FX task, CORE0 starvation detection, reboot cause recorded in `/health.json`
