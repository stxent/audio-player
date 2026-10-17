# Audio Player for Microcontrollers

![C23](https://img.shields.io/badge/C-23-blue)
![CMake](https://img.shields.io/badge/CMake-3.21+-blue)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Audio Player is a lightweight audio playback solution tailored for
resource-constrained embedded systems. Built specifically for NXP LPC
microcontrollers, it supports both WAV and MP3 formats. It utilizes the
Helix MP3 decoder for MP3 playback. The project uses the I²S bus for
high-quality audio output and features native integration with
TLV320AIC310x series audio codecs.

## Supported Platforms

The project is tested and optimized for the following NXP microcontrollers:

* **NXP LPC175x** Series - Cortex-M3 based MCUs
* **NXP LPC43xx** Series - Cortex-M4 based MCUs (supports both flashless and
  flash-based variants)

## System Requirements

To build and use the Audio Player project, the following software components
are required:

* **ARM GCC 13 or newer** — Arm GNU Toolchain, required for
  Cortex-M embedded targets.
* **CMake 3.21 or newer** — used for configuring and generating build
  systems across platforms.
* **Kconfiglib** or **kconfig-frontends** — tools for interactive Halm library
  configuration via `menuconfig`.
* **dfu-util** — required for flashing firmware via USB bootloader.

## Getting Started

### Cloning the Repository

To begin working with the project, clone the repository using
the following commands:

```sh
git clone https://github.com/stxent/audio-player.git
cd audio-player
git submodule update --init --recursive
```

### Building the Project

1. Building for LPC17xx Development Board

```sh
mkdir build && cd build
cmake .. -DPLATFORM=LPC17XX -DBOARD=lpc17xx_devkit -DCMAKE_TOOLCHAIN_FILE=libs/xcore/toolchains/cortex-m3.cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_MP3=ON -DUSE_DFU=ON -DUSE_LTO=OFF -DUSE_WDT=ON
make
```

2. Building for LPC43xx Development Board

```sh
mkdir build && cd build
cmake .. -DPLATFORM=LPC43XX -DBOARD=lpc43xx_devkit -DCMAKE_TOOLCHAIN_FILE=libs/xcore/toolchains/cortex-m4.cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_MP3=ON -DUSE_DFU=ON -DUSE_LTO=OFF -DUSE_WDT=ON
make
```

3. Building for LPC43xx with NOR flash support

```sh
mkdir build && cd build
cmake .. -DPLATFORM=LPC43XX -DBOARD=lpc43xx_devkit -DCMAKE_TOOLCHAIN_FILE=libs/xcore/toolchains/cortex-m4.cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_MP3=ON -DUSE_DFU=ON -DUSE_LTO=OFF -DUSE_NOR=ON -DUSE_WDT=ON
make
```

### Build Artifacts and Deployment

After successful compilation, the firmware files are organized as follows:

* All firmware files are stored in the board directory within
  the build directory.
* The main application firmware is located in `application.bin`.
* DFU firmwares are available in the
  [dpm-examples](https://github.com/stxent/dpm-examples.git) project.

### Flashing Instructions

The generated firmware can be flashed using various tools:

* **LPC-Link** programmer
* **J-Link** programmer
* **dfu-util** for DFU-enabled builds (root access may be required)

To flash the device, follow these steps:

* Flash the DFU firmware using tools such as LPC-Link or J-Link.
* Load the application firmware using `dfu-util`:
  ```sh
  dfu-util -R -D application.bin
  ```

## Configuration Flags

You can customize the build using the following CMake options:

* **CMAKE_BUILD_TYPE** — defines the build configuration type.
* **ENABLE_MP3** — enables MP3 decoding support.
* **USE_DBG** — enables detailed debug messages and activates performance
  profiling features.
* **USE_DFU** — configures firmware to use DFU (Device Firmware Update)
  memory layout, requires compatible DFU bootloader.
* **USE_LTO** — enables Link Time Optimization.
* **USE_NOR** — places firmware in external NOR flash memory.
* **USE_WDT** — Activates Watchdog Timer functionality.

Note: when using **USE_NOR**, **USE_LTO** must be disabled.
