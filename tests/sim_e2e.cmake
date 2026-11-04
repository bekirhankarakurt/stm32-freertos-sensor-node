# End-to-end check of the simulation: commands in, decoded telemetry out.
# Inputs: SIM, DECODE, WORK
file(MAKE_DIRECTORY ${WORK})
set(cmds ${WORK}/commands.bin)
set(stream ${WORK}/stream.bin)

# 1. Encode commands with the decoder's own framing (it has nothing to read from /dev/null).
execute_process(COMMAND ${DECODE} --port /dev/null --tx ${cmds} --send version --send period=50 --send period=5
                RESULT_VARIABLE rc_tx)

# 2. Run the firmware for 1.5 s with those commands on its UART input.
execute_process(COMMAND ${SIM} --duration-ms 1500 --input ${cmds} --output ${stream} RESULT_VARIABLE rc_sim)
if(NOT rc_sim EQUAL 0)
    message(FATAL_ERROR "sn_sim exited with ${rc_sim}")
endif()

# 3. Decode and check the replies and the effect of the period change.
execute_process(COMMAND ${DECODE} --port ${stream} OUTPUT_VARIABLE out RESULT_VARIABLE rc_dec)
message("${out}")
foreach(expected "BOOT  v" "VERSION v" "ACK  cmd=0x11 status=ok" "ACK  cmd=0x11 status=out-of-range"
        "HEALTH" "[ok]")
    string(FIND "${out}" "${expected}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "missing '${expected}' in decoded stream")
    endif()
endforeach()
string(REGEX MATCHALL "TELEM" telem "${out}")
list(LENGTH telem n)
# 1.5 s at 50 ms per sample is ~30 records; 10 Hz alone would give ~15.
if(n LESS 22)
    message(FATAL_ERROR "expected the period change to raise the rate, got ${n} records")
endif()
