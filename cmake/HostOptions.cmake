# Host-only instrumentation (sanitizers, coverage) applied through an interface target.
add_library(sn_host_flags INTERFACE)
if(SN_SANITIZE)
    target_compile_options(sn_host_flags INTERFACE -fsanitize=${SN_SANITIZE} -fno-omit-frame-pointer
        -fno-sanitize-recover=all)
    target_link_options(sn_host_flags INTERFACE -fsanitize=${SN_SANITIZE})
endif()
if(SN_COVERAGE)
    target_compile_options(sn_host_flags INTERFACE --coverage -O0)
    target_link_options(sn_host_flags INTERFACE --coverage)
endif()
