# AGENTS.md

## Guidance

* Pay close attention to memory management – allocation, freeing, boundaries
* Pay close attention to thread safety
* Pay close attention to tasks management, work allocated to tasks, race conditions
* Prefer smart pointers, RAII over raw pointers and new/delete

## Environment

* Build: PlatformIO
* IDE: CLion, VSCode with PlatformIO extension
* OS: Windows for code editing, compiling only. Linux Ubuntu for compiling and updating boards
  * The board is connected to the Linux Ubuntu host via USB 
  * The Ubuntu host can also monitor and capture serial output
* Board: Raspberry Pi Pico 2 family – RP2350 CPU, two cores, 520KB RAM, 4MB Flash.
  * Particular boards targeted: Plasma 2350 W, Raspberry Pi Pico 2 W
* Compiler: GCC ARM Embedded, C++17 standard
* Framework: arduino-pico from https://github.com/earlephilhower/arduino-pico, locally at ~/.platformio/packages/framework-arduinopico
  * FreeRTOS kernel
* Libraries: FastLED, ArduinoJson
* WiFi: CYW43439 supports 802.11b/g/n WiFi and Bluetooth 4.2 BLE

## Project Summary
A sophisticated, feature-rich LED lighting effects controller for WS2811/WS2812 LED strips, built 
for Raspberry Pi Pico 2350 boards with WiFi and leveraging the awesome FastLED library. 
The project uses FreeRTOS to manage tasks. 
The RP2350 microcontroller sporting dual-core and increased RAM allows for
delivering smooth, complex lighting effects with web-based configuration, time-aware scheduling, 
and holiday-themed color palettes.

### Features

- **70+ Custom Light Effects**: Original animations and community-inspired patterns
- **Effect Registry**: Auto-registration system for modular effect management
- **Web Interface**: Real-time configuration via WiFi-enabled web server with JSON REST API
- **Smart Scheduling**: Time-aware dimming and automatic holiday detection
- **Multi-Core Architecture**: FreeRTOS-based task distribution across both RP2040 cores
- **OTA Updates**: Over-the-air firmware updates
- **Memory Monitoring**: Custom malloc wrappers with real-time heap tracking

