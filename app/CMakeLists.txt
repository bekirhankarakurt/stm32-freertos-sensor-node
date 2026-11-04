# Portable C++17 application logic (no RTOS, no hardware).
add_library(sn_app STATIC
    src/protocol.cpp
    src/filter.cpp
    src/processing.cpp
    src/health.cpp
    src/command.cpp)
target_include_directories(sn_app PUBLIC include)
target_link_libraries(sn_app PUBLIC sn_common)
sn_target_warnings(sn_app)
sn_target_embedded_cxx(sn_app)

# sn_add_node(<target> <freertos-target>): RTOS task wiring for one platform.
function(sn_add_node target freertos)
    add_library(${target} STATIC ${PROJECT_SOURCE_DIR}/app/rtos/node.cpp)
    target_include_directories(${target} PUBLIC ${PROJECT_SOURCE_DIR}/app/rtos)
    target_link_libraries(${target} PUBLIC sn_app ${freertos} sn_version)
    sn_target_warnings(${target})
    sn_target_embedded_cxx(${target})
endfunction()
