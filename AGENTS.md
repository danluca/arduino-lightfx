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
* Board: Arduino Nano RP2040 Connect – RP2040 CPU (dual Cortex-M0+ @ 133MHz), 264KB RAM (144KB FreeRTOS heap), 16MB Flash (4MB configured).
  * PlatformIO environments: `rp2040-rel` (release), `rp2040-dbg` (debug, Raspberry Pi Debug Probe)
  * Onboard PDM microphone, LSM6DSOX IMU, ECC608 crypto chip
* Compiler: GCC ARM Embedded, C++17 standard
* Framework: arduino-pico, fork https://github.com/danluca/arduino-pico (branch `feat/stable`) of https://github.com/earlephilhower/arduino-pico, locally at ~/.platformio/packages/framework-arduinopico
  * FreeRTOS kernel
* Libraries: FastLED (pinned 3.10.3), ArduinoJson, local libraries under `lib/` (WiFiNINA port, mDNS, REST web server, filesystem task, time, logging)
* WiFi: U-blox NINA-W102 (802.11b/g/n, 2.4GHz) via the local WiFiNINA library
* Secrets: `include/secrets.h` is git-ignored (template `include/secrets.h.example`) – WiFi credentials and the upload token `FW_AUTH_TOKEN`; never commit secrets

## Project Summary
A sophisticated, feature-rich LED lighting effects controller for WS2811/WS2812 LED strips, built 
for the Arduino Nano RP2040 Connect board with WiFi and leveraging the awesome FastLED library. 
The project uses FreeRTOS to manage tasks. 
The dual-core RP2040 microcontroller allows for
delivering smooth, complex lighting effects with web-based configuration, time-aware scheduling, 
and holiday-themed color palettes.

### Features

- **50 Custom Light Effects**: Original animations and community-inspired patterns
- **Effect Registry**: Auto-registration system for modular effect management
- **Web Interface**: Real-time configuration via WiFi-enabled web server with JSON REST API
- **Smart Scheduling**: Time-aware dimming and automatic holiday detection
- **Multi-Core Architecture**: FreeRTOS-based task distribution across both RP2040 cores
- **OTA Updates**: Over-the-air firmware updates
- **Memory Monitoring**: Real-time heap and task stack tracking from FreeRTOS statistics

