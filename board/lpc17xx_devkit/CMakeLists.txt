# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Audio box versions
option(AUDIOBOX_V1 "Build project for Audio Box V1." OFF)

# Set version variables
set(VERSION_HW_MAJOR 1 PARENT_SCOPE)
set(VERSION_HW_MINOR 0 PARENT_SCOPE)

# Device version, version 2 is used by default
set(DEVICE_VERSION 2 CACHE STRING "Device version, possible values are 1 and 2.")

set(FLASH_BASE 0)
math(EXPR FLASH_SIZE "256 * 1024")
math(EXPR DFU_LENGTH "16 * 1024")

# Generate linker script
if(USE_DFU)
    math(EXPR ROM_LENGTH "${FLASH_SIZE} - ${DFU_LENGTH}")
    math(EXPR ROM_ORIGIN "${FLASH_BASE} + ${DFU_LENGTH}")
else()
    math(EXPR ROM_LENGTH "${FLASH_SIZE}")
    math(EXPR ROM_ORIGIN "${FLASH_BASE}")
endif()

# Linker script for a main application and tests
configure_file("memory.ld" "${PROJECT_BINARY_DIR}/memory.ld")

set(FLAGS_BOARD "-DDEVICE_VERSION=${DEVICE_VERSION}" PARENT_SCOPE)
set(FLAGS_LINKER "--specs=nosys.specs --specs=nano.specs -Wl,--gc-sections" PARENT_SCOPE)
