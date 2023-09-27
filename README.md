# arduino-lightfx
LightFX Arduino code repository for LED strips using WS2811 chip - light effects tailored to my house setup.

## Pre-Requisites
### Tools
* Install Python 3.11
  * On Windows: Go to MS Store and look for _Python 3.11_ application. Install it as any other store app
* Install PlatformIO Core (CLI)
  * [Follow instructions](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html#local-download-macos-linux-windows) in the docs - I used the local download approach. Use what makes sense for your platform.
* Install a great IDE
  * [CLion](https://www.jetbrains.com/clion/download) is a great one - however, commercial product (but REALLY Great)
  * [VS Code](https://code.visualstudio.com/) is a great for someone starting with PlatformIO - recommended by the team. Freeware.
  * [Visual Studio Community](https://visualstudio.microsoft.com/downloads/) is also great, if you're doing already C/C++ development in VStudio. Freeware.
  * [Arduino IDE](https://www.arduino.cc/en/software) is the officially recommended IDE of the Arduino community (that runs on desktop; there is also a cloud based option that runs in a browser). Freeware.

Which IDE should you choose, ultimately? It depends on what are you comfortable with, the size of your project, etc.
For a project that fits into 1-2 `.ino` files, the official Arduino IDE is a great option. It's an all in one solution - editor, library manager, compiler, upload, serial monitor, debugger if eligible, etc.
As the project size increases, the IDE becomes more sluggish, the intellisense takes a noticeable while - and these are the times where intellisense is most valuable.
The Arduino IDE uses `.ino` files as an optimized C++ file set and processes them as follows:
* the file with the same name as the sketch (aka project) is the main file
* all other `.ino` files are concatenated into one in alphabetical order (likely appended to the end of the main file) and compiled that way
* if the `.ino` include other regular `.h` header files, it seems those are processed/embedded into the caller `.ino` files prior to concatenation.
* all files in the project are opened at once in the IDE (they cannot be closed)

Personally, as I am working with IntelliJ a lot, I prefer the CLion IDE as it is more responsive. It also uses the [PlatformIO](https://platformio.org) system, 
which brings a number of benefits:
* dependency management similar with NodeJS `package.json`
* automated project initialization, updates, adapting to IDE, etc. from CLI
* PlatformIO CLion plugin
* any number of files in the project, follows the standard C++ structure and processing


### Setup
To create a new project, follow the [instructions on PlatformIO](https://docs.platformio.org/en/stable/core/quickstart.html) site about getting started. There is great documentation
about integrations with IDEs, capabilities and configuration needed of supported boards (Arduino, Raspberry Pi, ESP32, etc).

# Overview
This project started as Arduino IDE based, then as number of files grew moved over to PlatformIO and CLion.

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

The Nano RP2040 board has a built-in PDM microphone, which has been enabled as a source of entropy for the random number generator - the PDM library takes about 25-30% 
of the board's available RAM (264kB) for the audio signal processing.

PIO availability helps a lot with [FastLED](https://github.com/FastLED/FastLED) library performance and effects consistent timings - creating and outputting the PWM 
signal for controlling the LEDs does not take main CPU cycles and is handled in the background.

## Design
The system is designed as a platform that hosts a number of light effects, has awareness of time and holiday in effect, and changes the current light effect on few criteria.
Each light effect is encapsulated in a subclass of a base abstract class that defines the API. At bootstrap, each effect auto-registers itself into the global light effects
registry in order to make itself available.

The time awareness gives several benefits:
* auto-detection of holiday (based on day/month) that drives the color palettes appropriate for the occasion
* dimming rules for the time of day (brightness reduction of the LED strip, as we head further into the night)

### Multi-threading
Several dedicated threads are defined in the system:
* Main thread runs the Web Server
  * as the Web Server implements REST-ful API using JSON data format, the Arduino JSON library needed more memory allocated 
  for a dedicated thread. On the main thread, however, no special memory provisions were needed - hence it was left running there.
* Thread 0 runs the light effects
* Thread 1 runs the microphone signal processing (PDM to PCM conversion)

### Configuration
A single LED controller is instantiated from `FastLED` library that runs on pin 25 (aka pin D2 on the pinout diagram) for PWM output.
The number of pixels configured in the system is 384 - this drives memory allocation for pixel arrays. The LED strip installed on the 
house are connected in a graph with the longest path at about 300 pixels. For WS2811 LED strips - running at 12V - a pixel consists of 3 LEDs.

I have used LED strips with a LED density of 30 LED/m - made by BTF-Lighting (part number WS28115M30LW65 on Amazon) - outdoor splash proof rated IP65.

### Board wrapper
Nano RP2040 Connect board operates internally at 3.3V, whereas the WS2811 LED strips operate with 12V for main power and 5V for command pin - a 
LED driver circuit surrounds the board. It uses the main 12V power supplied to the LED strips to scale it down to 5V (LM7805) in order to provide 
the board with power (note the Arduino board has its own voltage regulator onboard, which makes it accept external power from 5V to 21V, per the specs).
The PWM 3.3V signal is run through a level shifter (74HCT125) to 5V for the LED command driver.

This way, a single 3 pin connector is used to connect the board to the LED strip as its controller - it takes 12V power from the strip
and supplies PWM 5V signal to the LEDs control pin. 

## C++ standard
The current Arduino libraries are leveraging the **C++ 14** standard, hence the code can be written with this standard 
level in mind and IDE support can be configured accordingly (if needed).

The confirmation of which C++ standard is currently in use comes from a verbose build with PlatformIO - `pio run -v` after a clean.
Inspect the compiler `-std=xyz` argument. For instance, `-std=gnu++14` indicates C++ 14 standard is used.

Please see [C++ 14 reference](https://en.cppreference.com/w/cpp/14) for standard details.