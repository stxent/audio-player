Installation
------------

MCU Audio Player project supports playing MP3 and WAV files on LPC175x/LPC176x and LPC43xx MCU. It requires GNU toolchain for ARM Cortex-M processors and CMake version 3.21.

Quickstart
----------

Clone git repository:

```sh
git clone https://github.com/stxent/audio-player.git
cd audio-player
git submodule update --init --recursive
```

Build release version (DFU disabled) for LPC17xx development board:

```sh
mkdir build
cd build
cmake .. -DPLATFORM=LPC17XX -DBOARD=lpc17xx_devkit -DCMAKE_TOOLCHAIN_FILE=libs/xcore/toolchains/cortex-m3.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_MP3=ON -DUSE_LTO=OFF
make
```

Build release version for LPC43xx development board:

```sh
mkdir build
cd build
cmake .. -DPLATFORM=LPC43XX -DBOARD=lpc43xx_devkit -DCMAKE_TOOLCHAIN_FILE=libs/xcore/toolchains/cortex-m4.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_MP3=ON -DUSE_LTO=OFF
make
```

All firmwares are placed in a *board* directory inside the *build* directory. The Application firmware is placed in an application.hex file and may be flashed using a preferred tool, for example LPC-Link or J-Link. Versions for a DFU must be loaded using dfu-util (root access may be required):

```sh
dfu-util -R -D application.bin
```

Useful settings
---------------

* CMAKE_BUILD_TYPE — specifies the build type. Possible values are empty, Debug, Release, RelWithDebInfo and MinSizeRel.
* USE_DBG — enables debug messages and profiling.
* USE_DFU — links application and test firmwares using DFU memory layout even in Debug and RelWithDebInfo modes.
* USE_LTO — enables Link Time Optimization.
* USE_WDT — explicitly enables a watchdog timer for all build types.
