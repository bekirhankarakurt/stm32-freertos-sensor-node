cmake_minimum_required(VERSION 3.16)

project(stm32_freertos_sensor_node
    VERSION 1.0.0
    DESCRIPTION "STM32F401RE FreeRTOS sensor node with A/B bootloader"
    LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Warnings)
include(Version)
include(Dependencies)

if(CMAKE_CROSSCOMPILING)
    set(SN_FIRMWARE ON)
else()
    set(SN_FIRMWARE OFF)
endif()

if(NOT SN_FIRMWARE)
    option(SN_BUILD_TESTS "Build unit tests" ON)
    option(SN_BUILD_SIM "Build the FreeRTOS POSIX simulation" ON)
    option(SN_BUILD_TOOLS "Build host tools (decoder, uploader)" ON)
    set(SN_SANITIZE "" CACHE STRING "Sanitizers for host builds, e.g. address,undefined")
    option(SN_COVERAGE "Instrument host builds for gcov" OFF)
    include(HostOptions)
endif()

add_subdirectory(common)
add_subdirectory(app)

if(SN_FIRMWARE)
    add_subdirectory(firmware)
else()
    if(SN_BUILD_SIM)
        add_subdirectory(sim)
    endif()
    if(SN_BUILD_TOOLS)
        add_subdirectory(tools)
    endif()
    if(SN_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()
