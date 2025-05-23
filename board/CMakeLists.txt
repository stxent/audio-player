# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

if(NOT DEFINED BOARD)
    message(FATAL_ERROR "BOARD not defined")
endif()
if(NOT DEFINED PLATFORM)
    message(FATAL_ERROR "PLATFORM not defined")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/libs/xcore/cmake")
include(GetGitInfo)
include(ListDirectories)

add_subdirectory("${BOARD}")

# Generate version file
cmake_host_system_information(RESULT BUILD_HOSTNAME QUERY HOSTNAME)
string(TIMESTAMP BUILD_TIMESTAMP "%s")
git_commit_count(VERSION_SW_REVISION)
git_hash(VERSION_SW_HASH)
git_version(VERSION_SW_MAJOR VERSION_SW_MINOR)
configure_file("version_template.c" "${PROJECT_BINARY_DIR}/version.c")

# Find source files
file(GLOB_RECURSE APPLICATION_SOURCES "${BOARD}/application/*.c")
file(GLOB_RECURSE SHARED_SOURCES "${BOARD}/shared/*.c")
list(APPEND SHARED_SOURCES "${PROJECT_BINARY_DIR}/version.c")

# Shared package
add_library(shared ${SHARED_SOURCES})
target_compile_definitions(shared PUBLIC ${FLAGS_BOARD})
target_include_directories(shared PUBLIC "${BOARD}/shared")
target_link_options(shared PUBLIC SHELL:${FLAGS_CPU} SHELL:${FLAGS_LINKER})
# Shared library depends on includes from core object library
target_link_libraries(shared PUBLIC core dpm yaf)

if(USE_NOR)
    target_compile_definitions(shared PUBLIC -DENABLE_NOR)
endif()

set(EXECUTABLES "")

# Application package
list(APPEND EXECUTABLES "application")
set(application_DIR "${BOARD}/application")
set(application_SOURCES ${APPLICATION_SOURCES})

# Test packages
list_directories(TESTS_LIST "${CMAKE_CURRENT_SOURCE_DIR}/${BOARD}/tests")

foreach(TEST_NAME ${TESTS_LIST})
    file(GLOB_RECURSE TEST_SOURCES "${BOARD}/tests/${TEST_NAME}/*.c")
    list(APPEND EXECUTABLES "test_${TEST_NAME}")
    set(test_${TEST_NAME}_DIR "${BOARD}/tests/${TEST_NAME}")
    set(test_${TEST_NAME}_SOURCES ${TEST_SOURCES})
endforeach()

# Make application and test executables
foreach(EXECUTABLE_NAME ${EXECUTABLES})
    if("${BOARD}" STREQUAL "linux")
        set(EXECUTABLE_ARTIFACT "${EXECUTABLE_NAME}")
    else()
        set(EXECUTABLE_ARTIFACT "${EXECUTABLE_NAME}.elf")
    endif()

    add_executable(${EXECUTABLE_ARTIFACT} ${${EXECUTABLE_NAME}_SOURCES})
    target_include_directories(${EXECUTABLE_ARTIFACT} PRIVATE ${EXECUTABLE_NAME}_DIR)
    target_link_libraries(${EXECUTABLE_ARTIFACT} PRIVATE shared)

    if(USE_DBG)
        target_link_options(${EXECUTABLE_ARTIFACT} PRIVATE SHELL:"-Wl,--print-memory-usage")
    endif()
    if(USE_DFU)
        target_compile_definitions(${EXECUTABLE_ARTIFACT} PRIVATE -DENABLE_DFU)
    endif()
    if(USE_WDT)
        target_compile_definitions(${EXECUTABLE_ARTIFACT} PRIVATE -DENABLE_WDT)
    endif()

    if(NOT "${BOARD}" STREQUAL "linux")
        target_link_options(${EXECUTABLE_ARTIFACT} PRIVATE SHELL:-T"${PROJECT_BINARY_DIR}/memory.ld")

        add_custom_command(TARGET ${EXECUTABLE_ARTIFACT} POST_BUILD
                COMMAND "${CMAKE_OBJCOPY}" ${EXECUTABLE_ARTIFACT} -Obinary ${EXECUTABLE_NAME}.bin
        )
        add_custom_command(TARGET ${EXECUTABLE_ARTIFACT} POST_BUILD
                COMMAND "${CMAKE_OBJCOPY}" ${EXECUTABLE_ARTIFACT} -Oihex ${EXECUTABLE_NAME}.hex
        )
    else()
        if(${EXECUTABLE_NAME} MATCHES "test_.*")
            add_test(${EXECUTABLE_NAME} ${EXECUTABLE_ARTIFACT})
        endif()
    endif()
endforeach()
