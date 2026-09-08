# Third-party sources, pinned to release tags and fetched at configure time.
include(FetchContent)

set(SN_FREERTOS_TAG "V11.1.0" CACHE STRING "FreeRTOS-Kernel release tag")
set(SN_CMSIS_CORE_TAG "v5.9.0_20250520" CACHE STRING "CMSIS Core (STMicroelectronics mirror) tag")
set(SN_CMSIS_F4_TAG "v2.6.10" CACHE STRING "CMSIS device STM32F4 tag")

# Populate without adding the dependency's own CMake project.
macro(sn_fetch name)
    FetchContent_GetProperties(${name})
    if(NOT ${name}_POPULATED)
        if(CMAKE_VERSION VERSION_LESS 3.18)
            FetchContent_Populate(${name})
        else()
            FetchContent_MakeAvailable(${name})
        endif()
    endif()
endmacro()

FetchContent_Declare(freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG ${SN_FREERTOS_TAG}
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-add)

# sn_add_freertos(<target> <port-dir> <config-include-dir> [extra sources...])
function(sn_add_freertos target port_dir config_dir)
    sn_fetch(freertos_kernel)
    set(src ${freertos_kernel_SOURCE_DIR})
    add_library(${target} STATIC
        ${src}/tasks.c ${src}/queue.c ${src}/list.c
        ${src}/${port_dir}/port.c ${ARGN})
    target_include_directories(${target} SYSTEM PUBLIC
        ${src}/include ${src}/${port_dir} ${config_dir})
    set_target_properties(${target} PROPERTIES FREERTOS_SOURCE_DIR ${src})
endfunction()

function(sn_fetch_cmsis)
    FetchContent_Declare(cmsis_core
        GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis_core.git
        GIT_TAG ${SN_CMSIS_CORE_TAG}
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR do-not-add)
    FetchContent_Declare(cmsis_device_f4
        GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis_device_f4.git
        GIT_TAG ${SN_CMSIS_F4_TAG}
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR do-not-add)
    sn_fetch(cmsis_core)
    sn_fetch(cmsis_device_f4)
    add_library(sn_cmsis INTERFACE)
    target_include_directories(sn_cmsis SYSTEM INTERFACE
        ${cmsis_core_SOURCE_DIR}/Core/Include
        ${cmsis_device_f4_SOURCE_DIR}/Include)
    target_compile_definitions(sn_cmsis INTERFACE STM32F401xE)
endfunction()
