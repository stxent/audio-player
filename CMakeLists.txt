# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

cmake_minimum_required(VERSION 3.21)
project(AudioPlayer C)

option(ENABLE_MP3 "Enable MP3 support." ON)
set(PATH_LENGTH 64 CACHE STRING "Maximum length of a track path in bytes.")

option(USE_DBG "Enable debug messages." OFF)
option(USE_DFU "Use memory layout for the bootloader." OFF)
option(USE_LTO "Enable Link Time Optimization." OFF)
option(USE_NOR "Use memory layout for external flash memory." OFF)
option(USE_WDT "Enable watchdog timer." OFF)

# Default compiler flags
set(FLAGS_PROJECT "-fdata-sections -ffunction-sections -pedantic -Wall -Wextra -Wshadow")
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS OFF)

if(USE_NOR)
    set(FLAGS_PROJECT "${FLAGS_PROJECT} -mslow-flash-data")
endif()

if(USE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_C_COMPILE_OPTIONS_IPO -flto=auto -ffat-lto-objects)
    endif()
endif()

# Configure XCORE library
set(XCORE_ENTRY_LENGTH 80 CACHE STRING "" FORCE)
add_subdirectory(libs/xcore xcore)

if(USE_DBG)
    set(CONFIG_CORE_CORTEX_MEMORY_DEBUG ON)
    set(CONFIG_CORE_CORTEX_MEMORY_PATTERN "0xDEADBEEF")
    set(CONFIG_GENERIC_WQ_LOAD ON)
    set(CONFIG_GENERIC_WQ_PM OFF)
endif()

# Configure HALM library, HALM_CONFIG_FILE should be defined
set(HALM_CONFIG_FILE "${PROJECT_SOURCE_DIR}/board/${BOARD}/halm.config" CACHE INTERNAL "" FORCE)
add_subdirectory(libs/halm halm)

# Configure YAF library
set(YAF_THREADS OFF CACHE STRING "" FORCE)
add_subdirectory(libs/yaf yaf)

# Configure DPM library
add_subdirectory(libs/dpm dpm)

# Configure Helix MP3 library
if(ENABLE_MP3)
    set(HELIX_MP3_C_FLAGS "${FLAGS_CPU} ${FLAGS_PROJECT}" CACHE INTERNAL "" FORCE)
    set(HELIX_MP3_LTO ${USE_LTO} CACHE STRING "" FORCE)
    set(HELIX_MP3_STATIC ON CACHE STRING "" FORCE)
    add_subdirectory(libs/helix_mp3 helix_mp3)
endif()

# Add project directories
add_subdirectory(core)
add_subdirectory(board)
