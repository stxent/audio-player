# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Find source files
file(GLOB_RECURSE CORE_SOURCES "*.c")

# Core package
add_library(core ${CORE_SOURCES})
target_compile_definitions(core PUBLIC -DCONFIG_PATH_LENGTH=${PATH_LENGTH})
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(core PUBLIC halm yaf)

if(ENABLE_MP3)
    target_compile_definitions(core PUBLIC -DCONFIG_ENABLE_MP3)
    target_link_libraries(core PUBLIC helix_mp3)
endif()
