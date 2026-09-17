# Host tools: telemetry decoder and firmware uploader (C++17).
add_library(sn_tools STATIC serial_port.cpp format.cpp uploader.cpp)
target_include_directories(sn_tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sn_tools PUBLIC sn_app sn_common)
sn_target_warnings(sn_tools)
sn_target_embedded_cxx(sn_tools)

add_executable(sn_decode sn_decode.cpp)
target_link_libraries(sn_decode PRIVATE sn_tools)
sn_target_warnings(sn_decode)
sn_target_embedded_cxx(sn_decode)

add_executable(sn_upload sn_upload.cpp)
target_link_libraries(sn_upload PRIVATE sn_tools)
sn_target_warnings(sn_upload)
sn_target_embedded_cxx(sn_upload)
