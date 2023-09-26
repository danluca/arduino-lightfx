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
Several light effects are original, many others are inspired from great example repositories:
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

## Details
The target board is an [Arduino RP 2040](https://docs.arduino.cc/hardware/nano-rp2040-connect) feature packed and powerful in a small package.
It sports a Wi-Fi module that has been leveraged to host a little web-server to aid in configuring the light effects.

## C++ standard
The current Arduino libraries are leveraging the **C++ 14** standard, hence the code can be written with this standard 
level in mind and IDE support can be configured accordingly (if needed).

The confirmation of which C++ standard is currently in use comes from a verbose build with PlatformIO - `pio run -v` after a clean.
Inspect the compiler `-std=xyz` argument. For instance, `-std=gnu++14` indicates C++ 14 standard is used.

Please see [C++ 14 reference](https://en.cppreference.com/w/cpp/14) for standard details.