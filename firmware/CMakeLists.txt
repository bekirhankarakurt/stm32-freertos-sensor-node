# Cross-compiled firmware: bootloader, application images for slots A/B, QEMU image.
sn_fetch_cmsis()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(SN_BME280_BUS "I2C" CACHE STRING "BME280 transport on the Nucleo board (I2C or SPI)")
set_property(CACHE SN_BME280_BUS PROPERTY STRINGS I2C SPI)

set(SN_IMGTOOL ${PROJECT_SOURCE_DIR}/tools/imgtool.py)
set(SN_LINKER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/linker)

# Register-level drivers (C11).
add_library(sn_drivers STATIC
    drivers/src/rcc.c drivers/src/gpio.c drivers/src/uart.c drivers/src/i2c.c drivers/src/spi.c
    drivers/src/timebase.c drivers/src/iwdg.c drivers/src/flash.c drivers/src/reset.c)
target_include_directories(sn_drivers PUBLIC drivers/include)
target_link_libraries(sn_drivers PUBLIC sn_cmsis sn_common)
sn_target_warnings(sn_drivers)

add_library(sn_startup OBJECT startup/startup_stm32f401re.c)
target_link_libraries(sn_startup PRIVATE sn_cmsis)
sn_target_warnings(sn_startup)

# FreeRTOS Cortex-M4F port + RTOS application.
sn_add_freertos(sn_freertos_cm4f portable/GCC/ARM_CM4F ${CMAKE_CURRENT_SOURCE_DIR}/config)
sn_add_node(sn_node_target sn_freertos_cm4f)

# sn_add_image(<name>): .bin/.hex next to the ELF, map file and size report.
function(sn_finish_elf target)
    target_link_options(${target} PRIVATE -Wl,-Map=$<TARGET_FILE_DIR:${target}>/${target}.map
        -Wl,--print-memory-usage -L${SN_LINKER_DIR})
    set_target_properties(${target} PROPERTIES SUFFIX ".elf")
endfunction()

# ---- Bootloader -------------------------------------------------------------
add_executable(sn_bootloader bootloader/main.c $<TARGET_OBJECTS:sn_startup>)
target_link_libraries(sn_bootloader PRIVATE sn_drivers)
target_link_options(sn_bootloader PRIVATE -T${SN_LINKER_DIR}/bootloader.ld)
sn_target_warnings(sn_bootloader)
sn_finish_elf(sn_bootloader)
add_custom_command(TARGET sn_bootloader POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:sn_bootloader> sn_bootloader.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:sn_bootloader> sn_bootloader.hex
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# ---- Applications -----------------------------------------------------------
# sn_add_app(<target> <board-source> <slot A|B> [defines...])
function(sn_add_app target board slot)
    if(slot STREQUAL "A")
        set(SN_SLOT_ADDR 0x08020000)
    else()
        set(SN_SLOT_ADDR 0x08040000)
    endif()
    set(SN_SLOT_NAME ${slot})
    set(ld ${CMAKE_CURRENT_BINARY_DIR}/${target}.ld)
    configure_file(${SN_LINKER_DIR}/app.ld.in ${ld} @ONLY)

    add_executable(${target} app/main.c app/image_header.c app/cxx_support.cpp ${board}
        board/board_common.c $<TARGET_OBJECTS:sn_startup>)
    target_include_directories(${target} PRIVATE board)
    target_compile_definitions(${target} PRIVATE SN_SLOT_ADDR=${SN_SLOT_ADDR}U ${ARGN})
    target_link_libraries(${target} PRIVATE sn_node_target sn_drivers sn_version)
    target_link_options(${target} PRIVATE -T${ld})
    set_target_properties(${target} PROPERTIES LINK_DEPENDS ${ld})
    sn_target_warnings(${target})
    sn_target_embedded_cxx(${target})
    sn_finish_elf(${target})

    # Sign the raw image, then rebuild ELF/HEX from it so every format carries the final header.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> ${target}.unsigned.bin
        COMMAND ${Python3_EXECUTABLE} ${SN_IMGTOOL} sign ${target}.unsigned.bin ${target}.bin
                --header-out ${target}.header.bin
        COMMAND ${CMAKE_OBJCOPY} --update-section .image_header=${target}.header.bin
                $<TARGET_FILE:${target}> $<TARGET_FILE:${target}>
        COMMAND ${CMAKE_OBJCOPY} -I binary -O ihex --change-addresses ${SN_SLOT_ADDR}
                ${target}.bin ${target}.hex
        COMMAND ${CMAKE_COMMAND} -E remove ${target}.unsigned.bin ${target}.header.bin
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

if(SN_BME280_BUS STREQUAL "SPI")
    set(_bus_def SN_BME280_SPI)
else()
    set(_bus_def SN_BME280_I2C)
endif()
sn_add_app(sn_app_a board/board_nucleo.c A ${_bus_def})
sn_add_app(sn_app_b board/board_nucleo.c B ${_bus_def})
sn_add_app(sn_app_qemu board/board_qemu.c A)

# Complete flash images: bootloader + slot A.
foreach(variant IN ITEMS a qemu)
    set(app sn_app_${variant})
    add_custom_target(sn_full_${variant} ALL
        COMMAND ${Python3_EXECUTABLE} ${SN_IMGTOOL} merge sn_full_${variant}.bin
                sn_bootloader.bin@0x08000000 ${app}.bin@0x08020000
        DEPENDS sn_bootloader ${app}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Merging bootloader and ${app} into sn_full_${variant}.bin")
endforeach()

add_custom_target(size_report ALL
    COMMAND ${CMAKE_SIZE} -B $<TARGET_FILE:sn_bootloader> $<TARGET_FILE:sn_app_a> $<TARGET_FILE:sn_app_b>
            $<TARGET_FILE:sn_app_qemu>
    DEPENDS sn_bootloader sn_app_a sn_app_b sn_app_qemu
    COMMENT "Firmware size report")
