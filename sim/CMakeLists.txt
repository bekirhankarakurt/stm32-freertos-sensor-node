# Whole application on Linux: FreeRTOS POSIX port + simulated board.
find_package(Threads REQUIRED)

sn_add_freertos(sn_freertos_posix portable/ThirdParty/GCC/Posix ${CMAKE_CURRENT_SOURCE_DIR})
get_target_property(_frt sn_freertos_posix FREERTOS_SOURCE_DIR)
target_sources(sn_freertos_posix PRIVATE ${_frt}/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c)
target_include_directories(sn_freertos_posix SYSTEM PUBLIC ${_frt}/portable/ThirdParty/GCC/Posix/utils)
target_link_libraries(sn_freertos_posix PUBLIC Threads::Threads sn_host_flags)
# The POSIX port relies on GNU/POSIX declarations (sigaction, clock_gettime, ...).
set_target_properties(sn_freertos_posix PROPERTIES C_EXTENSIONS ON)

sn_add_node(sn_node_sim sn_freertos_posix)

add_executable(sn_sim main_sim.c board_sim.c)
target_include_directories(sn_sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sn_sim PRIVATE sn_node_sim)
sn_target_warnings(sn_sim)
