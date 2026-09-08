# Version and source revision baked into images and host tools.
find_package(Git QUIET)
set(SN_GIT_HASH "0x00000000")
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE _sn_git OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _sn_git_rc)
    if(_sn_git_rc EQUAL 0 AND _sn_git MATCHES "^[0-9a-f]+$")
        set(SN_GIT_HASH "0x${_sn_git}")
    endif()
endif()

add_library(sn_version INTERFACE)
target_compile_definitions(sn_version INTERFACE
    SN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}U
    SN_VERSION_MINOR=${PROJECT_VERSION_MINOR}U
    SN_VERSION_PATCH=${PROJECT_VERSION_PATCH}U
    SN_GIT_HASH=${SN_GIT_HASH}UL)
