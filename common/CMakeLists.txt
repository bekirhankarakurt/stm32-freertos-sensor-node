# Portable C11 core shared by bootloader, application, simulation and host tools.
add_library(sn_common STATIC
    src/crc.c
    src/le.c
    src/cobs.c
    src/frame.c
    src/ringbuf.c
    src/flash_geometry.c
    src/image.c
    src/boot_control.c
    src/update_server.c
    src/bme280.c
    src/bme280_sim.c)
target_include_directories(sn_common PUBLIC include)
sn_target_warnings(sn_common)

if(NOT SN_FIRMWARE)
    target_sources(sn_common PRIVATE src/flash_sim.c)
    target_link_libraries(sn_common PUBLIC sn_host_flags)
endif()
