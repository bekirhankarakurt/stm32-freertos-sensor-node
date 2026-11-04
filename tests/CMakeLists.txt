# Host unit tests (dependency-free harness, one executable per suite).
function(sn_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE ${ARGN})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    sn_target_warnings(${name})
    sn_target_embedded_cxx(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

sn_add_test(test_codec sn_common)
sn_add_test(test_boot sn_common)
sn_add_test(test_bme280 sn_common)
sn_add_test(test_app sn_app)

if(SN_BUILD_TOOLS)
    sn_add_test(test_update sn_tools)
    sn_add_test(test_tools sn_tools)

    add_executable(test_imgtool test_imgtool.cpp)
    target_link_libraries(test_imgtool PRIVATE sn_tools)
    sn_target_warnings(test_imgtool)
    sn_target_embedded_cxx(test_imgtool)

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        set(work ${CMAKE_CURRENT_BINARY_DIR}/imgtool)
        add_test(NAME imgtool_template
                 COMMAND ${CMAKE_COMMAND} -E make_directory ${work})
        add_test(NAME imgtool_make
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/make_template.py
                         ${work}/unsigned.bin 0x08040000)
        add_test(NAME imgtool_sign
                 COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/imgtool.py sign
                         ${work}/unsigned.bin ${work}/signed.bin)
        add_test(NAME imgtool_info
                 COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/imgtool.py info ${work}/signed.bin)
        add_test(NAME imgtool_cross_check COMMAND test_imgtool ${work}/signed.bin 0x08040000)
        set_tests_properties(imgtool_template PROPERTIES FIXTURES_SETUP imgdir)
        set_tests_properties(imgtool_make PROPERTIES FIXTURES_REQUIRED imgdir FIXTURES_SETUP unsigned)
        set_tests_properties(imgtool_sign PROPERTIES FIXTURES_REQUIRED unsigned FIXTURES_SETUP signed)
        set_tests_properties(imgtool_info imgtool_cross_check PROPERTIES FIXTURES_REQUIRED signed)
    endif()
endif()

if(SN_BUILD_SIM AND SN_BUILD_TOOLS)
    add_test(NAME sim_end_to_end
             COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:sn_sim> -DDECODE=$<TARGET_FILE:sn_decode>
                     -DWORK=${CMAKE_CURRENT_BINARY_DIR}/sim_e2e -P ${CMAKE_CURRENT_SOURCE_DIR}/sim_e2e.cmake)
    set_tests_properties(sim_end_to_end PROPERTIES TIMEOUT 30)
endif()
