# RP2040 LightFX Project Context

## Project Overview
This is an RP2040-based lighting effects controller project using the Arduino framework and PlatformIO. The target platform is the Nano RP2040 Connect board.

## Context for Agent interactions
- always include the following files and folders for context when interacting with the agent:
  - `src` folder for source code
  - `include` for header files
  - `lib` for libraries
  - `platformio.ini` for project configuration
  - `scripts` for support of PowerShell build and deploy scripts
  - `*.ps1` files for build and deploy
  - `build_info.py` for build info generation
  - `www` for web interface

## Technical Constraints
- **Limited Heap Memory**: Only 128KB heap available - always be mindful of memory allocations
- **Real-time Performance**: LED effects need to run smoothly without blocking
- **Memory Monitoring**: We have malloc wrappers and memory metrics tracking in place
- **Re-entrant Code**: Some code needs to be re-entrant safe

## Development Guidelines
The code is written in C++ and uses the Arduino framework, with Raspberry Pi Pico SDK as a backend.
FreeRTOS is used for multitasking. 

### Thread Safety
- Avoid blocking code in interrupt handlers
- Avoid blocking code in the main loop
- Prefer messaging threads over blocking code

### Memory Management
- Always use the custom memory allocation wrappers when available
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
- Main development branch: `dev/12-fxe`
- Release branch: `rel/nanorp2040connect`
- Use descriptive commit messages

## Build & Deploy
- Use provided PowerShell scripts: `build.ps1`, `clean.ps1`, `update.ps1`, `ota_upgrade.ps1`
- Serial logging available via `seriallog.ps1`
- Build info is automatically generated via `build_info.py`
- when running builds inline as part of the agent, always redirect the output to a 
  file in `logs` folder. Create the folder if it doesn't exist.

## Key Areas
- LED effects implementation
- Web interface (www/)
- OTA update support
- Memory allocation wrappers
- Stats and monitoring
