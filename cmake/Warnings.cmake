# Strict warning set applied to all first-party targets.
function(sn_target_warnings target)
    set(common_flags -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wcast-align -Wundef
        -Wdouble-promotion -Wformat=2 -Wnull-dereference)
    if(SN_WERROR)
        list(APPEND common_flags -Werror)
    endif()
    target_compile_options(${target} PRIVATE
        ${common_flags}
        $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes -Wmissing-prototypes>
        $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual>)
endfunction()

option(SN_WERROR "Treat warnings as errors" OFF)

# Embedded C++ profile: no exceptions, no RTTI, no thread-safe statics.
function(sn_target_embedded_cxx target)
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -fno-threadsafe-statics>)
endfunction()
