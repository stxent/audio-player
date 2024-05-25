# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set version variables
set(VERSION_HW_MAJOR 2 PARENT_SCOPE)
set(VERSION_HW_MINOR 0 PARENT_SCOPE)

if(USE_NOR)
    math(EXPR FLASH_BASE "0x14000000")
    math(EXPR FLASH_SIZE "4 * 1024 * 1024")
    math(EXPR DFU_LENGTH "128 * 1024")
else()
    math(EXPR FLASH_BASE "0x1A000000")
    math(EXPR FLASH_SIZE "256 * 1024")
    math(EXPR DFU_LENGTH "32 * 1024")
endif()

# Generate linker script
if(USE_DFU)
    math(EXPR ROM_LENGTH "${FLASH_SIZE} - ${DFU_LENGTH}")
    math(EXPR ROM_ORIGIN "${FLASH_BASE} + ${DFU_LENGTH}")
else()
    math(EXPR ROM_LENGTH "${FLASH_SIZE}")
    math(EXPR ROM_ORIGIN "${FLASH_BASE}")
endif()

# Linker script for a main application and tests
if(USE_NOR)
    configure_file("memory_nor.ld" "${PROJECT_BINARY_DIR}/memory.ld")
else()
    configure_file("memory.ld" "${PROJECT_BINARY_DIR}/memory.ld")
endif()

set(FLAGS_LINKER "--specs=nosys.specs --specs=nano.specs -Wl,--gc-sections" PARENT_SCOPE)
