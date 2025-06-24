# Lucas LightFx
LightFX Arduino code repository for LED strips using WS2811 chip - light effects tailored to my house setup.

## Dev Env
### Tools
* Install Python 3.12
  * On Windows: Go to MS Store and look for _Python 3.12_ application. Install it as any other store app
* Install PlatformIO Core (CLI)
  * [Follow instructions](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html#local-download-macos-linux-windows) in the docs - I used the local download approach. Use what makes sense for your platform.
* Install an IDE
  * [CLion](https://www.jetbrains.com/clion/download) is a great one - however, commercial product (but REALLY Great)
  * [VS Code](https://code.visualstudio.com/) is a great for someone starting with PlatformIO - recommended by the team. Freeware.
* (optional) Enable an AI assistant

### Setup
This project is focused on Raspberry Pi microcontrollers - in particular RP2040, RP2350.
The board of choice - for which all the code has been targeted - is [Arduino Nano RP 2040 Connect](https://docs.arduino.cc/hardware/nano-rp2040-connect). Other RP2040 based boards 
can work as well (see e.g. Pimoroni, Adafruit, SparkFun, etc.) with minimal modifications in the code - mainly to disable peripherals these boards may not have.
A board with WiFi connectivity is strongly recommended.

# Overview
The backbone of controlling the LEDs is the [FastLED](https://github.com/FastLED/FastLED) library.
Several light effects are original, many others are inspired from great example repositories 
(many thanks to their respective owners for sharing - it has helped me learn a LOT!):
* https://github.com/atuline/FastLED-Demos
* https://github.com/atuline/FastLED-SoundReactive
* https://github.com/chemdoc77
* https://github.com/marmilicious
* https://github.com/marcmerlin
* https://github.com/davepl/DavesGarageLEDSeries
* https://github.com/s-marley/FastLED-basics
* https://github.com/AaronLiddiment
* https://github.com/evilgeniuslabs
* https://github.com/jasoncoon
* https://github.com/brimshot/quickPatterns

## Board
The target board is an [Arduino Nano RP 2040 Connect](https://docs.arduino.cc/hardware/nano-rp2040-connect) feature packed and powerful in a small package - built around dual core Raspberry Pi 2040 microcontroller.
It sports a Wi-Fi module that has been leveraged to host a little web-server to aid in configuring the light effects.

The Nano RP2040 board has a built-in PDM microphone, which has been leveraged in some effects - the PDM library takes about 20% 
of the board's available RAM (264kB) for the audio signal processing. Overall, this program uses about 35% of the available RAM - the rest of 15% are consumed 
by the OS, FastLED and light effects data.

PIO availability helps a lot with [FastLED](https://github.com/FastLED/FastLED) library performance and effects consistent timings - creating and outputting the PWM 
signal for controlling the LEDs does not take main CPU cycles and is handled in the background.

The RP2040 chip overall computing power (two ARM cores @ 133MHz) and RAM are leveraged with design features that might be challenging for a lesser configuration:
* multi-core multi-threading
* regular expression matching
* array manipulation
* sound processing
* complex math
* file system
* web connectivity
  * web server, JSON REST API
  * NTP connection for time sync

Note the code is not designed with portability in mind - quite the opposite and unapologetically. 
It is acknowledged that it sacrifices portability - we'll cross that bridge once we get there. 

## Design
The system is designed as a platform that hosts a number of light effects, has awareness of time and holiday in effect, and changes the current light effect on few criteria.
Each light effect is encapsulated in a subclass of a base abstract class that defines the API. At bootstrap, each effect auto-registers itself into the global light effects
registry in order to make itself available.

The time awareness gives several benefits:
* auto-detection of holiday (based on day/month) that drives the color palettes appropriate for the occasion
* dimming rules for the time of day (brightness reduction of the LED strip, as we head further into the night)

### Core Library
The code is built atop Earle F. Philhower's [Arduino-Pico](https://github.com/earlephilhower/arduino-pico) core, based on FreeRTOS kernel. Very efficient resource utilization, rich capabilities built-in, multi-core enabled.

The built-in [WiFi support in the Arduino-Pico](https://arduino-pico.readthedocs.io/en/latest/wifi.html) is more generic and portable, yet seemingly targeted at Pico W and arguably still a work in progress.
As such, it consumes quite more memory than what I'm used to see from WiFiNINA and it requires a different FW for the WiFi module (a U-blox NINA W102) to make it compliant with the lwIP layer of the core.

For all these reasons, I've chosen to make the [Arduino WiFiNINA](https://github.com/arduino-libraries/WiFiNINA?tab=readme-ov-file) library work with this core instead. The result is minimal RAM and Flash increase, good and reliable WiFi capability.
The downsides - the WiFi actions are bound to Core0 task; reading the IP address from the WiFi module (over SPI) hangs the whole system. I'm using static IP allocation and I do not mind running WiFi on Core0, other tasks aren't as strict. 

### Multi-threading
Several dedicated threads are defined in the system that take advantage of both cores:
* CORE 0 (main)
  * CORE0 (main task) runs the Web Server - the WiFi NINA library seems to work best with this affinity.
  * ALM task - processes time based alarms
  * FS task - file system interactions. As LittleFS library (basic file system implementation) is not thread safe, all file operations are channeled through a dedicated task that ensures single access at a time to filesystem.  
* CORE 1 (second core)
  * FX task runs the light effects
  * Mic task runs the microphone signal processing (PDM to PCM conversion)
  * CORE1 task runs the diagnostic actions - logging system info, measure temperature, etc.
#### Optimal Task Configuration
The Pico core configures by default the core tasks with 1024 bytes of stack space. In most applications this may be enough,
however, the amount is not configurable externally.
With the task allocation above, a more optimal stack depth is **3072** bytes for CORE0 and **1536** bytes for CORE1. These adjustments 
can be made in the file `~/.platformio/packages/framework-arduinopico/libraries/FreeRTOS/src/variantHooks.cpp` within the function `startFreeRTOS`.

### Configuration
A single LED controller is instantiated from `FastLED` library that runs on pin 25 (aka pin D2 on the pinout diagram) for PWM output.
The number of pixels is configured in the system through defines (there are few board and light strip configuration variants).
For instance, the LED strip installed on the house is connected in a graph with the longest path at about 300 pixels. 

Note for WS2811 LED strips - running at 12V - a pixel consists of 3 LEDs.

I have used LED strips with a LED density of 30 LED/m - made by BTF-Lighting (part number WS28115M30LW65 on Amazon) - outdoor splash proof rated IP65.

### Board wrapper
Nano RP2040 Connect board operates internally at 3.3V, whereas the [WS2811](https://datasheet.lcsc.com/lcsc/1810081420_Worldsemi-WS2811_C114581.pdf) LED strips 
operate with 12V for main power and 5V for command pin (5V for data/command is unconfirmed by the datasheet, experimentally it has been proven to work) - 
hence a LED driver circuit surrounds the board. 

It uses the main 12V power supplied to the LED strips to scale it down to 5V (LM7805) in order to provide the board with power (note the Arduino board has its 
own voltage regulator onboard, which makes it accept external power from 5V to 21V, per the specs).
The PWM 3.3V signal is run through a level shifter (74HCT125) to 5V for the LED command driver.

This way, a single 3 pin connector is used to connect the board to the LED strip as its controller - it takes 12V power from the strip
and supplies PWM 5V signal to the LEDs control pin. 

The electronic schematic and PCB designs can be found as [EasyEDA project](https://pro.easyeda.com/editor#id=9c50130b250b4c23b522b4ac978d99bf).

Note: Pimoroni's [Plasma Stick 2040 W](https://shop.pimoroni.com/products/plasma-stick-2040-w?variant=40359072301139) board is a good equivalent of this setup, albeit running at 5V.

## C++ standard
The current Arduino libraries are leveraging the **C++ 17** standard, hence the code can be written with this standard 
level in mind and IDE support can be configured accordingly (if needed).

The confirmation of which C++ standard is currently in use comes from a verbose build with PlatformIO - `pio run -v` after a clean.
Inspect the compiler `-std=xyz` argument. For instance, `-std=gnu++17` indicates C++ 17 standard is used.

Please see [C++ 17 reference](https://en.cppreference.com/w/cpp/17) for standard details.

## Debug
### Overview
The debugging with Arduino Nano RP2040 Connect board is still very much work in progress and not very stable. The steps below deviate from the 
mainstream of installing tools/packages out of the box and just work - as either more updates are made to the tools/packages, 
or I manage to put together a pull-request, etc. we'll eventually get back on mainstream.

The main issues worked around:
* OpenOCD debugger has no support for the flash chip on-board - Atmel AT25SF128A 16MB NOR Flash
* OpenOCD configuration scripts need tweaks in order to work with RaspberryPi Debug Probe and Serial Wire Debugging (SWD)
* Some libraries (e.g. FastLED) do not compile well in debug build configuration
* Platform does not compile well using latest toolchain in debug configuration

### Hardware Pre-Requisites
The [Raspberry Pi Debug Probe](https://www.adafruit.com/product/5699) is needed along with a couple of extra JST SH 3-pin SWD cables.
The Nano RP2040 board needs some soldering work to connect wires to the SWD pads on the back and make those available through a connector. 
Be very careful with the SWD pads - they are very fragile and can easily break and come off the PCB.

The debug serial port wires can be soldered directly onto the header respective pins.

[Raspberry Pi Debug Probe documentation](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html) is a good help to get started.

### Custom OpenOCD build
Assumptions:
* the code base is located at `~/Code/Arduino/repos/arduino-lightfx` on the Ubuntu machine
* the custom tools folder exists at `~/Code/Tools`
* current working folder is `~/Code/Arduino/repos/rp2040-lightfx` 

From Earle Philhower's [Pico Quick Toolchain](https://github.com/earlephilhower/pico-quick-toolchain/releases) download the 
latest [OpenOCD tool](https://github.com/earlephilhower/pico-quick-toolchain/releases/download/2.1.0-a/aarch64-linux-gnu.openocd-4d87f6dca.230911.tar.gz) 
(file `aarch64-linux-gnu.openocd-4d87f6dca.230911.tar.gz` for Ubuntu) - as of this writing, the latest is version 2.1.0-a from Sep 11, 2023.

Decompress this archive to `~/Code/Tools/openocd-rp2040-earle` folder.

_Note: It is also worth checking the platformio registry for [Earle Philhower's OpenOCD for RP2040](https://registry.platformio.org/tools/earlephilhower/tool-openocd-rp2040-earlephilhower) - 
check the published date (hover over it with mouse). For instance, as of this writing, the version on PlatformIO is from Feb 18, 2023 (version 5.100300.230216)_ 

Clone the [OpenOCD](https://github.com/raspberrypi/openocd.git) git repository for RP2040 and switch to branch `rp2040-v0.12.0`:

`git clone git@github.com/raspberrypi/openocd.git; git switch rp2040-v0.12.0`

In a code editor - VSCode - locate and open the file `./src/flash/nor/spi.c`. Insert the following line into `flash_devices` array:

`	FLASH_ID("atmel 25sf128a",          0x03, 0xeb, 0x02, 0xd8, 0xc7, 0x0001891f, 0x100, 0x1000,  0x1000000),	//Arduino Nano Connect RP2040`

[Build the code](https://github.com/raspberrypi/openocd/blob/rp2040-v0.12.0/README) - follow the instructions to install dependencies needed.
First time there are few extra commands to be run, afterward only run `make`:
```shell
cd openocd
./bootstrap
./configure
make
```
Copy/replace the openocd executable to the openocd package in your home folder:

```shell
cp ./src/openocd ~/Code/Tools/openocd-rp2040-earle/bin
```
### Platformio Debug Config
As indicated by the platform config file - `~/.platformio/platforms/raspberrypi/platform.json` - the OpenOCD package listed as a dependency is indicated below:
```json
"tool-openocd-raspberrypi": {
      "type": "uploader",
      "optional": true,
      "owner": "platformio",
      "version": "~2.1100.0"
    }
```
Ensure you have the `arduino-lightfx` repository open in VSCode and open the `platformio.ini` file - keep it open while running the commands below.

In the terminal, run the following commands one time to:
* remove the configured OpenOCD package as it will collide with the custom one we'll install
  * if this package hasn't been downloaded yet, run either `pio pkg update` (update project packages) or `pio run` (builds the project, and it will resolve dependencies)  
* create a PlatformIO symbolic link to our custom OpenOCD package for this dependency (update your user name)
 
```shell
rm -rf ~/.platformio/packages/tool-openocd-raspberrypi 
pio pkg install --tool "tool-openocd-raspberrypi=symlink:///home/dan/Code/Tools/openocd-rp2040-earle"
```
Note you can choose `file://` instead of `symlink://` prefix and the custom OpenOCD will be copied into PlatformIO packages location instead of linked to your home folder.

One of the side effects of this command is to re-write the platformio.ini file and remove all comments - in VSCode simply press CTRL+Z to undo those changes (this is why we keep the file open in editor).

Replace the debug environment with following (adjust for your home path):
```ini
[env:rp2040-dbg]
monitor_speed = 115200
; the other option is cmsis-dap for both debug_tool and upload_protocol; picoprobe seems to be working better with RP2040 and the Pico Debug Probe
debug_tool = picoprobe
upload_protocol = picoprobe
; ignore all warnings - comment this when adding new libraries, or making major code changes
build_flags =
  -w
  -D configMAX_PRIORITIES=12
  -D configTIMER_QUEUE_LENGTH=24
  -D configRECORD_STACK_HIGH_ADDRESS=1
  -D configRUN_TIME_COUNTER_TYPE=uint64_t
  -D GIT_COMMIT=\"DBG\"
  -D GIT_COMMIT_SHORT=\"DBG\"
  -D GIT_BRANCH=\"DEV\"
  -D BUILD_TIME=\"${UNIX_TIME}\"
  -D LOGGING_ENABLED=1
  -D DEBUG_RP2040_PORT=Serial
;    -D DEBUG_RP2040_SPI=1
; See Readme - custom OpenOCD with Nano RP2040 Connect Atmel 16MB flash chip specifications added - built from source at https://github.com/raspberrypi/openocd.git branch rp2040-v0.12.0
platform_packages =
  tool-openocd-raspberrypi=symlink:///home/dan/Code/Tools/openocd-rp2040-earle
;;FOR PIO HOME INSPECTION only: to succeed building debug flavor, in efx_setup.h add #define FASTLED_ALLOW_INTERRUPTS 0 before including the PaletteFactory.h; see https://github.com/FastLED/FastLED/issues/1481
build_type = debug
debug_build_flags = -Og -ggdb -DFASTLED_ALLOW_INTERRUPTS=0
debug_speed = 5000
;;debug_svd_path=/home/dan/.platformio/packages/framework-arduino-mbed/svd/rp2040.svd
``` 

### Debug Session
VSCode is assumed to be the IDE with the codebase open.
* ensure the `env:rp2040-dbg (rp2040-lightfx)` shows up in the status bar for the profile (click on it and change it, if it lists something different - like rp2040-rel)
* switch to Debug view
* run the `PIO Debug` action. For reference, while the `launch.json` is updated automatically by PlatformIO plugin, the content below is an example of how configuration should look like:
```json
  {
  "type": "platformio-debug",
  "request": "launch",
  "name": "PIO Debug",
  "executable": "/home/dan/Code/Arduino/repos/rp2040-lightfx/.pio/build/rp2040-rel/firmware.elf",
  "projectEnvName": "rp2040-dbg",
  "toolchainBinDir": "/home/dan/.platformio/packages/toolchain-rp2040-earlephilhower/bin",
  "internalConsoleOptions": "openOnSessionStart",
  "svdPath": "/home/dan/.platformio/platforms/raspberrypi@src-ff76a3915224135aafad379817f41edd/misc/svd/rp2040.svd",
  "preLaunchTask": {
    "type": "PlatformIO",
    "task": "Pre-Debug"
    }
  },
```
* Note that upon finishing setting up the environment, the VSCode will show an error pop-up in the _Debug Console_ tab that states it cannot parse the SVD file.
The error is known and [talked about in the community](https://github.com/platformio/platform-raspberrypi/issues/21) (message is rather cryptic) and at this time 
we don't have a fix (suspect the PlatformIO parser as root cause - note other SVD files are loaded properly).
The effect is that _Peripherals_ panel in the debugger is not populated, CPU registries have generic names, etc. but it won't prevent the breakpoints, stepping through the code or variable inspection.
* The execution should stop somewhere in a main.cpp file and from there you can setup breakpoints in your own code 

