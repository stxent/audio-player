# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set version variables
set(VERSION_HW_MAJOR 1 PARENT_SCOPE)
set(VERSION_HW_MINOR 0 PARENT_SCOPE)

math(EXPR FLASH_BASE "0x00000000")
math(EXPR DFU_LENGTH "16 * 1024")

# Generate linker script
if(USE_DFU OR ${CMAKE_BUILD_TYPE} STREQUAL "Release" OR ${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel")
    math(EXPR ROM_LENGTH "256 * 1024 - ${DFU_LENGTH}")
    math(EXPR ROM_ORIGIN "${FLASH_BASE} + ${DFU_LENGTH}")
else()
    math(EXPR ROM_LENGTH "256 * 1024")
    math(EXPR ROM_ORIGIN "${FLASH_BASE}")
endif()

# Linker script for a main application and tests
configure_file("memory.ld" "${PROJECT_BINARY_DIR}/memory.ld")

set(FLAGS_LINKER "--specs=nosys.specs --specs=nano.specs -Wl,--gc-sections" PARENT_SCOPE)
