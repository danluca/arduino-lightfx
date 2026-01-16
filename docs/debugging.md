# Debug

NOTE: These instructions might be outdated. Use at your own risk.

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

