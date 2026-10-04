# RP2350 LightFX Project Context

## Project Overview
This is an RP2350-based lighting effects controller project using the Arduino framework and PlatformIO. The target platform is the Pimoroni Plasma2350 W board.

## Context for Agent interactions
- always include the following files and folders for context when interacting with the agent:
  - `src` folder for source code
  - `include` for header files
  - `lib` for libraries
  - `platformio.ini` for project configuration
  - `scripts` for support of PowerShell build and deploy scripts
  - `*.ps1` files for build and deploy
  - `scripts/pio_build.py` for build info generation
  - `www` for web interface
  - `docs/okf` knowledge bundle (architecture, tasks, REST API, known issues)

## Technical Constraints
- **Limited Heap Memory**: 192KB FreeRTOS heap (of 520KB SRAM, shared with the newlib and lwIP heaps) - always be mindful of memory allocations
- **Real-time Performance**: LED effects need to run smoothly without blocking
- **Memory Monitoring**: Heap and task stack metrics come from FreeRTOS statistics (`sysinfo_runtime.cpp`)
- **Re-entrant Code**: Some code needs to be re-entrant safe

## Development Guidelines
The code is written in C++ and uses the Arduino framework, with Raspberry Pi Pico SDK as a backend.
FreeRTOS is used for multitasking. 

### Thread Safety
- Avoid blocking code in interrupt handlers
- Avoid blocking code in the main loop
- Prefer messaging threads over blocking code

### Memory Management
- Be conscious of memory leaks - we've had to fix several
- Check memory metrics on the stats page after changes
- Avoid large stack allocations
- The project uses Heap 4 memory allocator to optimize for memory footprint

### Code Style
- Follow existing patterns in the codebase
- Use descriptive variable and function names
- Comment complex algorithms, especially for LED effects

### Testing
- Test memory allocations carefully
- Verify LED effects run smoothly without flickering
- Check performance on actual hardware when possible
- Monitor the stats page for memory issues

### Git Workflow
- Development branch: `dev/plasma2350w`
- The RP2350 (Raspberry Pi Pico 2 W) code uses `dev/plasma2350w` / `rel/plasma2350w`; shared code (effects, scheduling, `lib/`) may need porting between the two
- Use descriptive commit messages

## Build & Deploy
- Use provided PowerShell scripts: `build.ps1`, `clean.ps1`, `update.ps1`, `ota_upgrade.ps1`
- Serial logging available via `seriallog.ps1`
- Build info is automatically generated via `scripts/pio_build.py`
- when running builds inline as part of the agent, always redirect the output to a 
  file in `logs` folder. Create the folder if it doesn't exist.

## Key Areas
- LED effects implementation
- Web interface (www/)
- OTA update support
- Heap and stack monitoring
- Stats and monitoring
